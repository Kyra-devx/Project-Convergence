#pragma once

#include "convergence/voxel_dims.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace convergence {

struct Vertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float r;
    float g;
    float b;
};

static_assert(sizeof(Vertex) == sizeof(float) * 9U,
              "Vertex attributes must remain tightly packed.");

struct ChunkData {
    int chunk_x = 0;
    int chunk_z = 0;
    std::array<std::uint8_t, cvg::BLOCK_COUNT> blocks{};
};

class Chunk final {
public:
    static constexpr int kSizeX = cvg::SX;
    static constexpr int kSizeY = cvg::SY;
    static constexpr int kSizeZ = cvg::SZ;
    // Retained for code that refers to the horizontal chunk width.
    static constexpr int kSize = kSizeX;
    static constexpr std::size_t kBlockCount = cvg::BLOCK_COUNT;
    static constexpr std::uint8_t kAir = 0;
    static constexpr std::uint8_t kGrass = 1;
    static constexpr std::uint8_t kDirt = 2;
    static constexpr std::uint8_t kStone = 3;

    using NeighborLookup =
        std::function<std::uint8_t(int localX, int localY, int localZ)>;

    Chunk();
    explicit Chunk(ChunkData&& input);

    [[nodiscard]] int chunk_x() const noexcept;
    [[nodiscard]] int chunk_z() const noexcept;
    [[nodiscard]] bool contains(int x, int y, int z) const noexcept;
    [[nodiscard]] std::uint8_t block_at(int x, int y, int z) const noexcept;
    [[nodiscard]] bool set_block(int x, int y, int z,
                                 std::uint8_t block) noexcept;

    void mark_dirty() noexcept;
    [[nodiscard]] bool is_dirty() const noexcept;

    // CPU-only meshing. This class owns no OpenGL resources and is safe to
    // construct, move, and destroy independently of an EGL context.
    void build_mesh(const NeighborLookup& neighbor_lookup = {});
    [[nodiscard]] const std::vector<Vertex>& mesh_vertices() const noexcept;
    [[nodiscard]] std::size_t visible_face_count() const noexcept;
    [[nodiscard]] std::uint64_t mesh_revision() const noexcept;

private:
    [[nodiscard]] static std::size_t index_of(int x, int y, int z) noexcept;

    ChunkData data_{};
    std::vector<Vertex> mesh_vertices_;
    bool mesh_dirty_ = true;
    std::uint64_t mesh_revision_ = 0U;
};

}  // namespace convergence
