#pragma once

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

class Chunk final {
public:
    static constexpr int kSize = 16;
    static constexpr std::size_t kBlockCount =
        static_cast<std::size_t>(kSize) * kSize * kSize;
    static constexpr std::uint8_t kAir = 0;
    static constexpr std::uint8_t kGrass = 1;
    static constexpr std::uint8_t kDirt = 2;
    static constexpr std::uint8_t kStone = 3;

    // Receives neighbor coordinates in this chunk's local coordinate space.
    // The callback is invoked only when a face crosses a chunk boundary.
    using NeighborLookup =
        std::function<std::uint8_t(int localX, int localY, int localZ)>;

    Chunk();

    [[nodiscard]] bool contains(int x, int y, int z) const noexcept;
    [[nodiscard]] std::uint8_t block_at(int x, int y, int z) const noexcept;
    [[nodiscard]] bool set_block(int x, int y, int z,
                                 std::uint8_t block) noexcept;

    // Missing neighbors are air. A supplied lookup enables cross-chunk culling.
    void build_mesh(const NeighborLookup& neighbor_lookup = {});

    [[nodiscard]] const std::vector<Vertex>& mesh_vertices() const noexcept;
    [[nodiscard]] std::size_t visible_face_count() const noexcept;

private:
    [[nodiscard]] static std::size_t index_of(int x, int y, int z) noexcept;

    std::array<std::uint8_t, kBlockCount> blocks_{};
    std::vector<Vertex> mesh_vertices_;
};

}  // namespace convergence
