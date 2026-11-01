#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#include "Camera.h"
#include "WorldGenerator.h"
#include "convergence/chunk.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
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
std::vector<convergence::Vertex> g_world_vertices;
std::unique_ptr<Camera> g_camera;

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

void build_generated_world(std::int64_t seed) {
    g_world_generator = std::make_unique<WorldGenerator>(seed);
    g_world_vertices.clear();

    constexpr int kVerticalChunkCount =
        (WorldGenerator::MAX_TERRAIN_HEIGHT +
         convergence::Chunk::kSize - 1) / convergence::Chunk::kSize;
    std::array<std::uint8_t, convergence::Chunk::kBlockCount> blocks{};

    // B.4 renders one X/Z chunk column. Extra Y sections expose the surface
    // above Y=16; horizontal chunk streaming remains part of B.5.
    for (int chunkY = 0; chunkY < kVerticalChunkCount; ++chunkY) {
        g_world_generator->generateChunk(0, chunkY, 0, blocks.data());
        convergence::Chunk chunk;

        for (int x = 0; x < convergence::Chunk::kSize; ++x) {
            for (int y = 0; y < convergence::Chunk::kSize; ++y) {
                for (int z = 0; z < convergence::Chunk::kSize; ++z) {
                    const std::size_t index =
                        (static_cast<std::size_t>(x) *
                             convergence::Chunk::kSize + y) *
                            convergence::Chunk::kSize + z;
                    const std::uint8_t block = blocks[index];
                    if (block != convergence::Chunk::kAir) {
                        (void)chunk.set_block(x, y, z, block);
                    }
                }
            }
        }

        chunk.build_mesh();
        const float yOffset = static_cast<float>(
            chunkY * convergence::Chunk::kSize);
        const auto& localVertices = chunk.mesh_vertices();

        // The mesher treats missing neighbor chunks as air. Remove only the
        // horizontal faces between contiguous vertical sections so no flat
        // internal planes remain in the rendered terrain.
        for (std::size_t face = 0; face + 5U < localVertices.size(); face += 6U) {
            const convergence::Vertex& first = localVertices[face];
            if (first.ny != 0.0F) {
                const float planeY = first.y + yOffset;
                const int boundaryY = static_cast<int>(planeY);
                if (planeY == static_cast<float>(boundaryY) &&
                    boundaryY > 0 &&
                    boundaryY % convergence::Chunk::kSize == 0) {
                    float centerX = 0.0F;
                    float centerZ = 0.0F;
                    for (std::size_t i = 0; i < 6U; ++i) {
                        centerX += localVertices[face + i].x / 6.0F;
                        centerZ += localVertices[face + i].z / 6.0F;
                    }
                    const float terrainHeight =
                        g_world_generator->getTerrainHeight(centerX, centerZ);
                    if (static_cast<float>(boundaryY) < terrainHeight) {
                        continue;
                    }
                }
            }

            for (std::size_t i = 0; i < 6U; ++i) {
                convergence::Vertex worldVertex = localVertices[face + i];
                worldVertex.y += yOffset;
                g_world_vertices.push_back(worldVertex);
            }
        }
    }

    __android_log_print(
        ANDROID_LOG_INFO, kLogTag,
        "Generated terrain seed=%lld: %zu visible faces, %zu vertices across %d vertical sections.",
        static_cast<long long>(seed), g_world_vertices.size() / 6U,
        g_world_vertices.size(), kVerticalChunkCount);
}

void upload_chunk_mesh() {
    if (g_world_vertices.empty()) {
        log_error("Cannot upload an empty chunk mesh.");
        return;
    }

    const std::vector<convergence::Vertex>& vertices = g_world_vertices;

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(convergence::Vertex)),
        vertices.data(),
        GL_STATIC_DRAW
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

    if (g_vao == 0 || g_vbo == 0) {
        log_error("Failed to create the chunk VAO/VBO.");
        g_vertex_count = 0;
    }
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
    g_world_generator.reset();
    std::vector<convergence::Vertex>().swap(g_world_vertices);
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

    build_generated_world(12345);
    upload_chunk_mesh();
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

    if (g_program == 0 || g_vao == 0 || g_vertex_count <= 0 ||
        g_camera == nullptr || g_world_generator == nullptr) {
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

    const float safe_aspect = std::max(aspect, 0.001F);
    const glm::mat4 mvp =
        g_camera->getProjectionMatrix(safe_aspect) * g_camera->getViewMatrix();

    glUseProgram(g_program);
    glUniformMatrix4fv(g_mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_vertex_count);
    glBindVertexArray(0);
}
