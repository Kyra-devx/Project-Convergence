#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#include "convergence/chunk.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

namespace {

constexpr char kLogTag[] = "ProjectConvergence";
constexpr float kPi = 3.14159265358979323846F;

std::unique_ptr<convergence::Chunk> g_demo_chunk;
GLuint g_program = 0;
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLint g_mvp_location = -1;
GLsizei g_vertex_count = 0;
std::array<float, 16> g_mvp{};

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
    const int bytes_read = AAsset_read(asset, source.data(), source.size());
    AAsset_close(asset);

    if (bytes_read != asset_length) {
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

void build_demo_chunk() {
    if (g_demo_chunk != nullptr) {
        return;
    }

    g_demo_chunk = std::make_unique<convergence::Chunk>();

    // Three stepped layers create a visible block stack for the B.2 prototype.
    for (int x = 2; x < 14; ++x) {
        for (int z = 2; z < 14; ++z) {
            for (int y = 0; y < 3; ++y) {
                (void)g_demo_chunk->set_block(
                    x, y, z, convergence::Chunk::kStone);
            }
        }
    }

    for (int x = 3; x < 13; ++x) {
        for (int z = 3; z < 13; ++z) {
            for (int y = 3; y < 6; ++y) {
                (void)g_demo_chunk->set_block(
                    x, y, z, convergence::Chunk::kDirt);
            }
        }
    }

    for (int x = 4; x < 12; ++x) {
        for (int z = 4; z < 12; ++z) {
            for (int y = 6; y < 9; ++y) {
                (void)g_demo_chunk->set_block(
                    x, y, z, convergence::Chunk::kGrass);
            }
        }
    }

    g_demo_chunk->build_mesh();
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "B.2 mesh built: %zu visible faces, %zu vertices.",
        g_demo_chunk->visible_face_count(),
        g_demo_chunk->mesh_vertices().size()
    );
}

void upload_chunk_mesh() {
    if (g_demo_chunk == nullptr || g_demo_chunk->mesh_vertices().empty()) {
        log_error("Cannot upload an empty chunk mesh.");
        return;
    }

    const std::vector<convergence::Vertex>& vertices =
        g_demo_chunk->mesh_vertices();

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
}

struct Vec3 {
    float x;
    float y;
    float z;
};

Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalize(Vec3 value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.00001F) {
        return {0.0F, 0.0F, 0.0F};
    }
    return {value.x / length, value.y / length, value.z / length};
}

std::array<float, 16> perspective(float fov_radians, float aspect,
                                  float near_plane, float far_plane) {
    std::array<float, 16> matrix{};
    const float f = 1.0F / std::tan(fov_radians * 0.5F);
    matrix[0] = f / aspect;
    matrix[5] = f;
    matrix[10] = (far_plane + near_plane) / (near_plane - far_plane);
    matrix[11] = -1.0F;
    matrix[14] = (2.0F * far_plane * near_plane) /
                 (near_plane - far_plane);
    return matrix;
}

std::array<float, 16> look_at(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 forward = normalize(subtract(center, eye));
    const Vec3 side = normalize(cross(forward, up));
    const Vec3 corrected_up = cross(side, forward);

    return {{
        side.x, corrected_up.x, -forward.x, 0.0F,
        side.y, corrected_up.y, -forward.y, 0.0F,
        side.z, corrected_up.z, -forward.z, 0.0F,
        -dot(side, eye), -dot(corrected_up, eye), dot(forward, eye), 1.0F,
    }};
}

std::array<float, 16> multiply(const std::array<float, 16>& left,
                               const std::array<float, 16>& right) {
    std::array<float, 16> result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int index = 0; index < 4; ++index) {
                result[column * 4 + row] +=
                    left[index * 4 + row] * right[column * 4 + index];
            }
        }
    }
    return result;
}

void update_camera(int width, int height) {
    const float aspect = static_cast<float>(width) /
                         static_cast<float>(height);
    const auto projection = perspective(50.0F * kPi / 180.0F, aspect,
                                        0.1F, 100.0F);
    const auto view = look_at(
        {23.0F, 18.0F, 27.0F},
        {8.0F, 4.0F, 8.0F},
        {0.0F, 1.0F, 0.0F}
    );
    g_mvp = multiply(projection, view);
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_MainActivity_nativeOnSurfaceCreated(
    JNIEnv* env,
    jclass /* clazz */,
    jobject asset_manager
) {
    // OpenGL object names belong to the current EGL context. Recreate them
    // whenever GLSurfaceView reports a new surface/context.
    g_program = 0;
    g_vao = 0;
    g_vbo = 0;
    g_vertex_count = 0;
    g_mvp_location = -1;

    glClearColor(0.035F, 0.055F, 0.085F, 1.0F);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    AAssetManager* manager = AAssetManager_fromJava(env, asset_manager);
    g_program = create_program(manager);
    if (g_program == 0) {
        log_error("Renderer initialization stopped because shaders failed.");
        return;
    }

    g_mvp_location = glGetUniformLocation(g_program, "uMVP");
    if (g_mvp_location < 0) {
        log_error("Shader uniform uMVP was not found.");
        return;
    }

    build_demo_chunk();
    upload_chunk_mesh();
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_MainActivity_nativeOnSurfaceChanged(
    JNIEnv* /* env */,
    jclass /* clazz */,
    jint width,
    jint height
) {
    if (width > 0 && height > 0) {
        glViewport(0, 0, width, height);
        update_camera(width, height);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_MainActivity_nativeOnDrawFrame(
    JNIEnv* /* env */,
    jclass /* clazz */
) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (g_program == 0 || g_vao == 0 || g_vertex_count <= 0) {
        return;
    }

    glUseProgram(g_program);
    glUniformMatrix4fv(g_mvp_location, 1, GL_FALSE, g_mvp.data());
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_vertex_count);
    glBindVertexArray(0);
}
