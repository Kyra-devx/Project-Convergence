#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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

class Chunk final {
public:
    static constexpr int kSize = 16;
    static constexpr std::size_t kBlockCount =
        static_cast<std::size_t>(kSize) * kSize * kSize;
    static constexpr std::uint8_t kAir = 0;
    static constexpr std::uint8_t kGrass = 1;
    static constexpr std::uint8_t kDirt = 2;
    static constexpr std::uint8_t kStone = 3;

    Chunk();

    [[nodiscard]] bool contains(int x, int y, int z) const noexcept;
    [[nodiscard]] std::uint8_t block_at(int x, int y, int z) const noexcept;
    [[nodiscard]] bool set_block(int x, int y, int z, std::uint8_t block) noexcept;

    // Rebuilds CPU-side geometry. Chunk-edge faces remain visible until neighbor
    // chunk data is supplied by the future chunk manager.
    void build_mesh();

    [[nodiscard]] const std::vector<Vertex>& mesh_vertices() const noexcept;
    [[nodiscard]] std::size_t visible_face_count() const noexcept;

private:
    [[nodiscard]] static std::size_t index_of(int x, int y, int z) noexcept;

    std::array<std::uint8_t, kBlockCount> blocks_{};
    std::vector<Vertex> mesh_vertices_;
};

}  // namespace convergence
