#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#include "Camera.h"
#include "ChunkManager.h"
#include "WorldGenerator.h"
#include "convergence/chunk.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace {

constexpr char kLogTag[] = "ProjectConvergence";
constexpr float kMaxDeltaTimeSeconds = 0.1F;

std::unique_ptr<WorldGenerator> g_world_generator;
std::unique_ptr<ChunkManager> g_chunk_manager;
std::unique_ptr<Camera> g_camera;
std::uint64_t g_uploaded_mesh_revision =
    std::numeric_limits<std::uint64_t>::max();

GLuint g_program = 0;
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLint g_mvp_location = -1;
GLsizei g_vertex_count = 0;

std::atomic<jlong> g_last_frame_time_ms{0};

struct PendingInput {
    float look_dx = 0.0F;
    float look_dy = 0.0F;
    float joy_forward = 0.0F;
    float joy_strafe = 0.0F;
};

std::mutex g_input_mutex;
PendingInput g_pending_input;

void log_error(const char* message) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", message);
}

std::string read_asset(AAssetManager* manager, const char* path) {
    if (manager == nullptr) {
        log_error("Android AssetManager is null.");
        return {};
    }

    AAsset* asset = AAssetManager_open(manager, path, AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "Unable to open shader asset: %s", path);
        return {};
    }

    const off_t asset_length = AAsset_getLength(asset);
    if (asset_length <= 0) {
        AAsset_close(asset);
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "Shader asset is empty: %s", path);
        return {};
    }

    std::string source(static_cast<std::size_t>(asset_length), '\0');
    std::size_t total_bytes_read = 0;
    bool read_complete = true;
    while (total_bytes_read < source.size()) {
        const int bytes_read = AAsset_read(
            asset,
            source.data() + total_bytes_read,
            source.size() - total_bytes_read
        );
        if (bytes_read <= 0) {
            read_complete = false;
            break;
        }
        total_bytes_read += static_cast<std::size_t>(bytes_read);
    }
    AAsset_close(asset);

    if (!read_complete) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "Could not read complete shader asset: %s", path);
        return {};
    }
    return source;
}

GLuint compile_shader(GLenum shader_type, const std::string& source) {
    const GLuint shader = glCreateShader(shader_type);
    if (shader == 0) {
        log_error("glCreateShader failed.");
        return 0;
    }

    const char* source_data = source.c_str();
    glShaderSource(shader, 1, &source_data, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> info_log(static_cast<std::size_t>(log_length) + 1U, '\0');
    glGetShaderInfoLog(shader, log_length, nullptr, info_log.data());
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "Shader compilation failed: %s", info_log.data());
    glDeleteShader(shader);
    return 0;
}

GLuint create_program(AAssetManager* manager) {
    const std::string vertex_source =
        read_asset(manager, "shaders/vertex_shader.glsl");
    const std::string fragment_source =
        read_asset(manager, "shaders/fragment_shader.glsl");

    if (vertex_source.empty() || fragment_source.empty()) {
        return 0;
    }

    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (vertex_shader == 0) {
        return 0;
    }

    const GLuint fragment_shader =
        compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        log_error("glCreateProgram failed.");
        return 0;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> info_log(static_cast<std::size_t>(log_length) + 1U, '\0');
    glGetProgramInfoLog(program, log_length, nullptr, info_log.data());
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "Shader program link failed: %s", info_log.data());
    glDeleteProgram(program);
    return 0;
}

bool upload_chunk_mesh() {
    if (g_chunk_manager == nullptr) {
        return false;
    }

    const std::vector<convergence::Vertex>& vertices =
        g_chunk_manager->mesh_vertices();
    if (vertices.size() >
        static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        log_error("Chunk mesh exceeds the OpenGL draw vertex-count limit.");
        g_vertex_count = 0;
        return false;
    }

    if (g_vao == 0) {
        glGenVertexArrays(1, &g_vao);
    }
    if (g_vbo == 0) {
        glGenBuffers(1, &g_vbo);
    }
    if (g_vao == 0 || g_vbo == 0) {
        log_error("Failed to create the streaming terrain VAO/VBO.");
        g_vertex_count = 0;
        return false;
    }
    if (vertices.size() > static_cast<std::size_t>(
            std::numeric_limits<GLsizeiptr>::max()) / sizeof(convergence::Vertex)) {
        log_error("Chunk mesh exceeds the OpenGL buffer-size limit.");
        g_vertex_count = 0;
        return false;
    }

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(convergence::Vertex)),
        vertices.empty() ? nullptr : vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(convergence::Vertex)),
        reinterpret_cast<const void*>(offsetof(convergence::Vertex, x))
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(convergence::Vertex)),
        reinterpret_cast<const void*>(offsetof(convergence::Vertex, nx))
    );
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        2, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(convergence::Vertex)),
        reinterpret_cast<const void*>(offsetof(convergence::Vertex, r))
    );
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    g_vertex_count = static_cast<GLsizei>(vertices.size());
    return true;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeCleanup(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    // Runs on GLSurfaceView's GL thread while its EGL context is current.
    if (g_vao != 0) {
        glDeleteVertexArrays(1, &g_vao);
        g_vao = 0;
    }
    if (g_vbo != 0) {
        glDeleteBuffers(1, &g_vbo);
        g_vbo = 0;
    }
    if (g_program != 0) {
        glDeleteProgram(g_program);
        g_program = 0;
    }

    g_mvp_location = -1;
    g_vertex_count = 0;
    g_last_frame_time_ms.store(0);
    g_chunk_manager.reset();  // Release chunk columns before their generator.
    g_world_generator.reset();
    g_uploaded_mesh_revision = std::numeric_limits<std::uint64_t>::max();
    g_camera.reset();

    std::lock_guard<std::mutex> lock(g_input_mutex);
    g_pending_input = {};
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeInit(
    JNIEnv* env,
    jobject /* thiz */,
    jobject asset_manager
) {
    // This callback runs on GLSurfaceView's GL thread. Recreate GL resources
    // whenever Android creates a new EGL context.
    g_program = 0;
    g_vao = 0;
    g_vbo = 0;
    g_vertex_count = 0;
    g_mvp_location = -1;
    g_last_frame_time_ms.store(0);
    g_chunk_manager.reset();
    g_world_generator.reset();
    g_uploaded_mesh_revision = std::numeric_limits<std::uint64_t>::max();

    {
        std::lock_guard<std::mutex> lock(g_input_mutex);
        g_pending_input = {};
    }

    glClearColor(0.035F, 0.055F, 0.085F, 1.0F);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    g_camera = std::make_unique<Camera>();
    // Spawn above the generated terrain; gravity settles the player onto it.
    g_camera->position = glm::vec3{8.0F, 80.0F, 14.0F};
    g_camera->yaw = -90.0F;
    g_camera->pitch = -25.0F;
    g_camera->fov = 70.0F;
    g_camera->movementSpeed = 5.0F;
    g_camera->lookSensitivity = 0.15F;

    AAssetManager* manager = AAssetManager_fromJava(env, asset_manager);
    g_program = create_program(manager);
    if (g_program == 0) {
        log_error("Renderer initialization stopped because shaders failed.");
        return;
    }

    g_mvp_location = glGetUniformLocation(g_program, "uMVP");
    if (g_mvp_location < 0) {
        log_error("Shader uniform uMVP was not found.");
        glDeleteProgram(g_program);
        g_program = 0;
        return;
    }

    constexpr std::int64_t kWorldSeed = 12345;
    g_world_generator = std::make_unique<WorldGenerator>(kWorldSeed);
    g_chunk_manager = std::make_unique<ChunkManager>(
        *g_world_generator, 4, ChunkManager::kDefaultLoadBudgetPerUpdate);
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "Chunk streaming initialized: seed=%lld, viewDistance=4.",
                        static_cast<long long>(kWorldSeed));
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeResize(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint width,
    jint height
) {
    if (width > 0 && height > 0) {
        glViewport(0, 0, width, height);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeHandleLook(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jfloat delta_x,
    jfloat delta_y
) {
    std::lock_guard<std::mutex> lock(g_input_mutex);
    // Accumulate motion until the GL thread consumes it. This prevents quick
    // UI-thread touch events from overwriting each other between rendered frames.
    g_pending_input.look_dx = std::clamp(
        g_pending_input.look_dx + delta_x, -2000.0F, 2000.0F);
    g_pending_input.look_dy = std::clamp(
        g_pending_input.look_dy + delta_y, -2000.0F, 2000.0F);
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeHandleJoystick(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jfloat forward,
    jfloat strafe
) {
    std::lock_guard<std::mutex> lock(g_input_mutex);
    g_pending_input.joy_forward = std::clamp(forward, -1.0F, 1.0F);
    g_pending_input.joy_strafe = std::clamp(strafe, -1.0F, 1.0F);
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeResetFrameClock(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    g_last_frame_time_ms.store(0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeRender(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jlong now_ms,
    jfloat aspect
) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (g_program == 0 || g_camera == nullptr ||
        g_world_generator == nullptr || g_chunk_manager == nullptr) {
        return;
    }

    const jlong previous_time_ms = g_last_frame_time_ms.exchange(now_ms);
    float delta_time = 0.0F;
    if (previous_time_ms > 0 && now_ms > previous_time_ms) {
        delta_time = static_cast<float>(now_ms - previous_time_ms) / 1000.0F;
    }
    delta_time = std::clamp(delta_time, 0.0F, kMaxDeltaTimeSeconds);

    PendingInput input;
    {
        std::lock_guard<std::mutex> lock(g_input_mutex);
        input = g_pending_input;
        g_pending_input.look_dx = 0.0F;
        g_pending_input.look_dy = 0.0F;
        // Joystick values remain active until the pointer is released.
    }

    g_camera->processLook(input.look_dx, input.look_dy);
    g_camera->processMovement(
        input.joy_forward, input.joy_strafe, delta_time,
        *g_world_generator);

    // Load the nearest missing columns incrementally to limit frame spikes.
    g_chunk_manager->update(g_camera->position.x, g_camera->position.z);
    const std::uint64_t currentMeshRevision =
        g_chunk_manager->mesh_revision();
    if (currentMeshRevision != g_uploaded_mesh_revision) {
        if (upload_chunk_mesh()) {
            g_uploaded_mesh_revision = currentMeshRevision;
        }
    }
    if (g_vao == 0 || g_vertex_count <= 0) {
        return;
    }

    const float safe_aspect = std::max(aspect, 0.001F);
    const glm::mat4 mvp =
        g_camera->getProjectionMatrix(safe_aspect) * g_camera->getViewMatrix();

    glUseProgram(g_program);
    glUniformMatrix4fv(g_mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_vertex_count);
    glBindVertexArray(0);
}
