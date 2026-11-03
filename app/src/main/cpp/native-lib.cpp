#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#include "Camera.h"
#include "Raycast.h"
#include "ChunkManager.h"
#include "WorldGenerator.h"
#include "convergence/chunk.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <sys/types.h>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

constexpr char kLogTag[] = "ProjectConvergence";
constexpr float kMaxDeltaTimeSeconds = 0.1F;

std::unique_ptr<WorldGenerator> g_world_generator;
std::unique_ptr<ChunkManager> g_chunk_manager;
std::unique_ptr<Camera> g_camera;

struct GpuChunkMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertex_count = 0;
    std::uint64_t mesh_revision = 0U;
};

std::unordered_map<ChunkCoord, GpuChunkMesh, ChunkCoordHash> g_gpu_chunks;

GLuint g_program = 0;
GLint g_mvp_location = -1;

GLuint g_highlight_program = 0;
GLuint g_highlight_vao = 0;
GLuint g_highlight_vbo = 0;
GLint g_highlight_mvp_location = -1;
GLint g_highlight_offset_location = -1;
RayHit g_current_hit{};
bool g_has_target = false;

std::atomic<jlong> g_last_frame_time_ms{0};

struct PendingInput {
    float look_dx = 0.0F;
    float look_dy = 0.0F;
    float joy_forward = 0.0F;
    float joy_strafe = 0.0F;
    int pending_action = 0;
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

GLuint create_program(AAssetManager* manager,
                      const char* vertex_asset,
                      const char* fragment_asset) {
    const std::string vertex_source = read_asset(manager, vertex_asset);
    const std::string fragment_source = read_asset(manager, fragment_asset);

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


void cleanup_highlight() noexcept {
    if (g_highlight_vbo != 0) {
        glDeleteBuffers(1, &g_highlight_vbo);
        g_highlight_vbo = 0;
    }
    if (g_highlight_vao != 0) {
        glDeleteVertexArrays(1, &g_highlight_vao);
        g_highlight_vao = 0;
    }
    if (g_highlight_program != 0) {
        glDeleteProgram(g_highlight_program);
        g_highlight_program = 0;
    }
    g_highlight_mvp_location = -1;
    g_highlight_offset_location = -1;
    g_current_hit = {};
    g_has_target = false;
}

bool initialize_highlight(AAssetManager* manager) {
    cleanup_highlight();
    g_highlight_program = create_program(
        manager,
        "shaders/highlight_vertex_shader.glsl",
        "shaders/highlight_fragment_shader.glsl");
    if (g_highlight_program == 0) return false;

    g_highlight_mvp_location =
        glGetUniformLocation(g_highlight_program, "uMVP");
    g_highlight_offset_location =
        glGetUniformLocation(g_highlight_program, "uOffset");
    if (g_highlight_mvp_location < 0 ||
        g_highlight_offset_location < 0) {
        log_error("Highlight shader uniforms were not found.");
        cleanup_highlight();
        return false;
    }

    // Slightly expanded cube avoids z-fighting while depth testing hides
    // edges that are behind the selected block.
    constexpr GLfloat kOutside = -0.003F;
    constexpr GLfloat kInside = 1.003F;
    constexpr GLfloat cubeLines[] = {
        kOutside,kOutside,kOutside, kInside,kOutside,kOutside,
        kInside,kOutside,kOutside, kInside,kOutside,kInside,
        kInside,kOutside,kInside, kOutside,kOutside,kInside,
        kOutside,kOutside,kInside, kOutside,kOutside,kOutside,

        kOutside,kInside,kOutside, kInside,kInside,kOutside,
        kInside,kInside,kOutside, kInside,kInside,kInside,
        kInside,kInside,kInside, kOutside,kInside,kInside,
        kOutside,kInside,kInside, kOutside,kInside,kOutside,

        kOutside,kOutside,kOutside, kOutside,kInside,kOutside,
        kInside,kOutside,kOutside, kInside,kInside,kOutside,
        kInside,kOutside,kInside, kInside,kInside,kInside,
        kOutside,kOutside,kInside, kOutside,kInside,kInside
    };

    glGenVertexArrays(1, &g_highlight_vao);
    glGenBuffers(1, &g_highlight_vbo);
    if (g_highlight_vao == 0 || g_highlight_vbo == 0) {
        log_error("Failed to allocate highlight VAO/VBO.");
        cleanup_highlight();
        return false;
    }

    glBindVertexArray(g_highlight_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_highlight_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeLines), cubeLines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

bool block_intersects_player(int blockX, int blockY, int blockZ,
                              const Camera& camera) noexcept {
    constexpr float kPlayerRadius = 0.30F;
    constexpr float kPlayerHeight = 1.80F;
    if (!std::isfinite(camera.position.x) ||
        !std::isfinite(camera.position.y) ||
        !std::isfinite(camera.position.z)) {
        return true;
    }

    const float playerMinX = camera.position.x - kPlayerRadius;
    const float playerMaxX = camera.position.x + kPlayerRadius;
    const float playerFeetY = camera.position.y - camera.eyeHeight;
    const float playerMaxY = playerFeetY + kPlayerHeight;
    const float playerMinZ = camera.position.z - kPlayerRadius;
    const float playerMaxZ = camera.position.z + kPlayerRadius;

    return static_cast<float>(blockX) < playerMaxX &&
           static_cast<float>(blockX + 1LL) > playerMinX &&
           static_cast<float>(blockY) < playerMaxY &&
           static_cast<float>(blockY + 1) > playerFeetY &&
           static_cast<float>(blockZ) < playerMaxZ &&
           static_cast<float>(blockZ + 1LL) > playerMinZ;
}

void apply_block_action(int action) {
    if (!g_has_target || g_chunk_manager == nullptr || g_camera == nullptr) {
        return;
    }

    if (action == 1) {
        if (!g_chunk_manager->set_world_block(
                g_current_hit.x, g_current_hit.y, g_current_hit.z,
                convergence::Chunk::kAir)) {
            log_error("Break action could not modify the targeted chunk.");
        }
        return;
    }

    if (action != 2 ||
        (g_current_hit.nx == 0 && g_current_hit.ny == 0 &&
         g_current_hit.nz == 0)) {
        return;  // No entering face (e.g. camera started inside a block).
    }

    const std::int64_t placeX =
        static_cast<std::int64_t>(g_current_hit.x) + g_current_hit.nx;
    const std::int64_t placeY =
        static_cast<std::int64_t>(g_current_hit.y) + g_current_hit.ny;
    const std::int64_t placeZ =
        static_cast<std::int64_t>(g_current_hit.z) + g_current_hit.nz;

    if (placeX < std::numeric_limits<int>::min() ||
        placeX > std::numeric_limits<int>::max() ||
        placeZ < std::numeric_limits<int>::min() ||
        placeZ > std::numeric_limits<int>::max() ||
        placeY < 0 || placeY >= cvg::SY) {
        return;
    }

    const int x = static_cast<int>(placeX);
    const int y = static_cast<int>(placeY);
    const int z = static_cast<int>(placeZ);
    if (g_chunk_manager->get_world_block(x, y, z) !=
            convergence::Chunk::kAir ||
        block_intersects_player(x, y, z, *g_camera)) {
        return;
    }

    if (!g_chunk_manager->set_world_block(
            x, y, z, convergence::Chunk::kGrass)) {
        log_error("Place action failed: destination chunk is not loaded.");
    }
}

void render_target_highlight(float aspect) {
    if (!g_has_target || g_highlight_program == 0 ||
        g_highlight_vao == 0 || g_camera == nullptr) {
        return;
    }

    const glm::mat4 mvp =
        g_camera->getProjectionMatrix(std::max(aspect, 0.001F)) *
        g_camera->getViewMatrix();

    glUseProgram(g_highlight_program);
    glUniformMatrix4fv(
        g_highlight_mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(
        g_highlight_offset_location,
        static_cast<float>(g_current_hit.x),
        static_cast<float>(g_current_hit.y),
        static_cast<float>(g_current_hit.z));

    // Keep depth testing enabled so the back edges are occluded by the block.
    glDepthMask(GL_FALSE);
    glLineWidth(1.0F);
    glBindVertexArray(g_highlight_vao);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glUseProgram(0);
}

void delete_gpu_chunk(GpuChunkMesh& mesh) noexcept {
    if (mesh.vbo != 0) {
        glDeleteBuffers(1, &mesh.vbo);
        mesh.vbo = 0;
    }
    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
        mesh.vao = 0;
    }
    mesh.vertex_count = 0;
}

void cleanup_gpu_chunks() noexcept {
    for (auto& entry : g_gpu_chunks) {
        delete_gpu_chunk(entry.second);
    }
    g_gpu_chunks.clear();
}

bool upload_changed_chunk_meshes() {
    if (g_chunk_manager == nullptr) {
        return false;
    }

    const std::vector<ChunkMeshView> views = g_chunk_manager->chunk_meshes();
    std::unordered_set<ChunkCoord, ChunkCoordHash> activeChunks;
    activeChunks.reserve(views.size());

    bool uploadSucceeded = true;
    for (const ChunkMeshView& view : views) {
        activeChunks.insert(view.coord);
        if (view.vertices == nullptr) {
            uploadSucceeded = false;
            continue;
        }

        const std::vector<convergence::Vertex>& vertices = *view.vertices;
        auto existing = g_gpu_chunks.find(view.coord);

        if (vertices.empty()) {
            if (existing != g_gpu_chunks.end()) {
                existing->second.vertex_count = 0;
                existing->second.mesh_revision = view.revision;
            }
            continue;
        }

        if (vertices.size() >
            static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
            log_error("Chunk exceeds the OpenGL draw vertex-count limit.");
            uploadSucceeded = false;
            continue;
        }
        if (vertices.size() >
            static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) /
                sizeof(convergence::Vertex)) {
            log_error("Chunk exceeds the OpenGL buffer-size limit.");
            uploadSucceeded = false;
            continue;
        }

        if (existing != g_gpu_chunks.end() &&
            existing->second.mesh_revision == view.revision) {
            continue;
        }

        if (existing == g_gpu_chunks.end()) {
            existing = g_gpu_chunks.emplace(view.coord, GpuChunkMesh{}).first;
        }

        GpuChunkMesh& gpu = existing->second;
        if (gpu.vao == 0) {
            glGenVertexArrays(1, &gpu.vao);
        }
        if (gpu.vbo == 0) {
            glGenBuffers(1, &gpu.vbo);
        }
        if (gpu.vao == 0 || gpu.vbo == 0) {
            log_error("Failed to allocate a chunk VAO/VBO.");
            uploadSucceeded = false;
            continue;
        }

        glBindVertexArray(gpu.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                vertices.size() * sizeof(convergence::Vertex)),
            vertices.data(),
            GL_DYNAMIC_DRAW
        );

        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(sizeof(convergence::Vertex)),
            reinterpret_cast<const void*>(offsetof(convergence::Vertex, x))
        );
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(sizeof(convergence::Vertex)),
            reinterpret_cast<const void*>(offsetof(convergence::Vertex, nx))
        );
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(
            2, 3, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(sizeof(convergence::Vertex)),
            reinterpret_cast<const void*>(offsetof(convergence::Vertex, r))
        );
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        gpu.vertex_count = static_cast<GLsizei>(vertices.size());
        gpu.mesh_revision = view.revision;
    }

    // Remove GPU resources for columns unloaded by ChunkManager.
    for (auto it = g_gpu_chunks.begin(); it != g_gpu_chunks.end();) {
        if (activeChunks.find(it->first) == activeChunks.end()) {
            delete_gpu_chunk(it->second);
            it = g_gpu_chunks.erase(it);
        } else {
            ++it;
        }
    }

    return uploadSucceeded;
}

void render_chunk_meshes(float aspect) {
    if (g_chunk_manager == nullptr || g_camera == nullptr) {
        return;
    }

    const float safeAspect = std::max(aspect, 0.001F);
    const glm::mat4 projection =
        g_camera->getProjectionMatrix(safeAspect);
    const glm::mat4 view = g_camera->getViewMatrix();

    glUseProgram(g_program);
    const std::vector<ChunkMeshView> meshes = g_chunk_manager->chunk_meshes();

    // Chunk vertex positions are local; apply the X/Z chunk origin per draw.
    for (const ChunkMeshView& mesh : meshes) {
        const auto gpu = g_gpu_chunks.find(mesh.coord);
        if (gpu == g_gpu_chunks.end() ||
            gpu->second.vao == 0 ||
            gpu->second.vertex_count <= 0) {
            continue;
        }

        const glm::vec3 origin{
            static_cast<float>(static_cast<std::int64_t>(mesh.coord.x) * cvg::SX),
            0.0F,
            static_cast<float>(static_cast<std::int64_t>(mesh.coord.z) * cvg::SZ)
        };
        const glm::mat4 model =
            glm::translate(glm::mat4{1.0F}, origin);
        const glm::mat4 mvp = projection * view * model;

        glUniformMatrix4fv(
            g_mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(gpu->second.vao);
        glDrawArrays(GL_TRIANGLES, 0, gpu->second.vertex_count);
    }

    glBindVertexArray(0);
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_VoxelRenderer_nativeCleanup(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    // Runs on GLSurfaceView's GL thread while its EGL context is current.
    // Stop and join the CPU worker before releasing its immutable generator.
    g_chunk_manager.reset();
    g_world_generator.reset();
    cleanup_highlight();
    cleanup_gpu_chunks();
    if (g_program != 0) {
        glDeleteProgram(g_program);
        g_program = 0;
    }

    g_mvp_location = -1;
    g_last_frame_time_ms.store(0);
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
    // This callback runs on GLSurfaceView's GL thread. Release any previous
    // runtime before recreating resources for a new EGL context.
    g_chunk_manager.reset();  // Joins the CPU worker before its generator is released.
    g_world_generator.reset();
    cleanup_highlight();
    cleanup_gpu_chunks();
    if (g_program != 0) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
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
    g_program = create_program(
        manager,
        "shaders/vertex_shader.glsl",
        "shaders/fragment_shader.glsl");
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

    if (!initialize_highlight(manager)) {
        log_error("Target highlight is unavailable; block actions remain enabled.");
    }

    constexpr std::int64_t kWorldSeed = 12345;
    g_world_generator = std::make_unique<WorldGenerator>(kWorldSeed);
    g_chunk_manager = std::make_unique<ChunkManager>(*g_world_generator, 4);
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
Java_com_convergence_voxel_VoxelRenderer_nativeRequestAction(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint action
) {
    if (action != 1 && action != 2) return;
    std::lock_guard<std::mutex> lock(g_input_mutex);
    g_pending_input.pending_action = static_cast<int>(action);
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
        g_pending_input.pending_action = 0;
        // Joystick values remain active until the pointer is released.
    }

    g_camera->processLook(input.look_dx, input.look_dy);
    g_camera->processMovement(
        input.joy_forward, input.joy_strafe, delta_time);

    // Sample the same heightmap as generation on the GL thread. This keeps
    // Camera independent from WorldGenerator and retains the B.4 floor clamp.
    const float terrainHeight = g_world_generator->getTerrainHeight(
        std::floor(g_camera->position.x),
        std::floor(g_camera->position.z));
    g_camera->applyGroundCollision(terrainHeight);

    // Worker output is collected and CPU meshes are built on this GL thread.
    g_chunk_manager->update(g_camera->position.x, g_camera->position.z);

    // Resolve the latest camera ray before applying one queued tap action.
    g_current_hit = raycastVoxels(
        g_camera->position,
        g_camera->getFrontVector(),
        5.0F,
        g_chunk_manager.get());
    g_has_target = g_current_hit.hit;
    if (input.pending_action != 0) {
        apply_block_action(input.pending_action);
        // Breaking removes the current target; placing makes the new block
        // targetable on this same frame.
        g_current_hit = raycastVoxels(
            g_camera->position,
            g_camera->getFrontVector(),
            5.0F,
            g_chunk_manager.get());
        g_has_target = g_current_hit.hit;
    }

    // Upload only dirty chunk revisions; changed CPU meshes are built by the
    // next bounded ChunkManager update, never by the input/UI thread.
    (void)upload_changed_chunk_meshes();
    render_chunk_meshes(aspect);
    render_target_highlight(aspect);
}
