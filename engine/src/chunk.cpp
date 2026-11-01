#include "convergence/chunk.hpp"

#include <array>

namespace convergence {
namespace {

using Vec3 = std::array<float, 3>;
using FaceCorners = std::array<Vec3, 4>;

constexpr std::array<Vec3, 6> kFaceNormals{{
    {{-1.0F, 0.0F, 0.0F}},
    {{1.0F, 0.0F, 0.0F}},
    {{0.0F, 1.0F, 0.0F}},
    {{0.0F, -1.0F, 0.0F}},
    {{0.0F, 0.0F, -1.0F}},
    {{0.0F, 0.0F, 1.0F}},
}};

constexpr std::array<std::array<int, 3>, 6> kNeighborOffsets{{
    {{-1, 0, 0}},
    {{1, 0, 0}},
    {{0, 1, 0}},
    {{0, -1, 0}},
    {{0, 0, -1}},
    {{0, 0, 1}},
}};

constexpr std::array<FaceCorners, 6> kFaceCorners{{
    // -X
    {{{{0.0F, 0.0F, 1.0F}},
      {{0.0F, 1.0F, 1.0F}},
      {{0.0F, 1.0F, 0.0F}},
      {{0.0F, 0.0F, 0.0F}}}},
    // +X
    {{{{1.0F, 0.0F, 0.0F}},
      {{1.0F, 1.0F, 0.0F}},
      {{1.0F, 1.0F, 1.0F}},
      {{1.0F, 0.0F, 1.0F}}}},
    // +Y
    {{{{0.0F, 1.0F, 0.0F}},
      {{0.0F, 1.0F, 1.0F}},
      {{1.0F, 1.0F, 1.0F}},
      {{1.0F, 1.0F, 0.0F}}}},
    // -Y
    {{{{0.0F, 0.0F, 1.0F}},
      {{0.0F, 0.0F, 0.0F}},
      {{1.0F, 0.0F, 0.0F}},
      {{1.0F, 0.0F, 1.0F}}}},
    // -Z (ordered counter-clockwise from outside the face)
    {{{{1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, 0.0F}},
      {{1.0F, 1.0F, 0.0F}}}},
    // +Z (ordered counter-clockwise from outside the face)
    {{{{0.0F, 0.0F, 1.0F}},
      {{1.0F, 0.0F, 1.0F}},
      {{1.0F, 1.0F, 1.0F}},
      {{0.0F, 1.0F, 1.0F}}}},
}};

[[nodiscard]] Vec3 color_for(std::uint8_t block, std::size_t face) noexcept {
    switch (block) {
        case Chunk::kGrass:
            if (face == 2) {
                return {{0.3F, 0.7F, 0.2F}};
            }
            if (face == 3) {
                return {{0.4F, 0.25F, 0.1F}};
            }
            return {{0.45F, 0.5F, 0.2F}};
        case Chunk::kDirt:
            return {{0.4F, 0.25F, 0.1F}};
        case Chunk::kStone:
            return {{0.5F, 0.5F, 0.5F}};
        default:
            return {{1.0F, 1.0F, 1.0F}};
    }
}

}  // namespace

Chunk::Chunk() {
    // Reserve the outer surface of a solid chunk; irregular terrain can grow as needed.
    mesh_vertices_.reserve(6U * kSize * kSize * 6U);
}

bool Chunk::contains(int x, int y, int z) const noexcept {
    return x >= 0 && x < kSize &&
           y >= 0 && y < kSize &&
           z >= 0 && z < kSize;
}

std::uint8_t Chunk::block_at(int x, int y, int z) const noexcept {
    if (!contains(x, y, z)) {
        return kAir;
    }
    return blocks_[index_of(x, y, z)];
}

bool Chunk::set_block(int x, int y, int z, std::uint8_t block) noexcept {
    if (!contains(x, y, z)) {
        return false;
    }
    blocks_[index_of(x, y, z)] = block;
    return true;
}

void Chunk::build_mesh() {
    mesh_vertices_.clear();

    constexpr std::array<std::array<std::size_t, 3>, 2> kTriangles{{
        {{0U, 1U, 2U}},
        {{0U, 2U, 3U}},
    }};

    for (int x = 0; x < kSize; ++x) {
        for (int y = 0; y < kSize; ++y) {
            for (int z = 0; z < kSize; ++z) {
                const std::uint8_t block = block_at(x, y, z);
                if (block == kAir) {
                    continue;
                }

                for (std::size_t face = 0; face < kFaceNormals.size(); ++face) {
                    const int nx = x + kNeighborOffsets[face][0];
                    const int ny = y + kNeighborOffsets[face][1];
                    const int nz = z + kNeighborOffsets[face][2];

                    // Out-of-chunk neighbors are treated as air in this
                    // single-chunk prototype. Neighbor chunk data will be
                    // consulted when chunk streaming is implemented.
                    if (contains(nx, ny, nz) && block_at(nx, ny, nz) != kAir) {
                        continue;
                    }

                    const Vec3 color = color_for(block, face);
                    const Vec3 normal = kFaceNormals[face];
                    const FaceCorners& corners = kFaceCorners[face];

                    for (const auto& triangle : kTriangles) {
                        for (const std::size_t corner_index : triangle) {
                            const Vec3& corner = corners[corner_index];
                            mesh_vertices_.push_back({
                                static_cast<float>(x) + corner[0],
                                static_cast<float>(y) + corner[1],
                                static_cast<float>(z) + corner[2],
                                normal[0],
                                normal[1],
                                normal[2],
                                color[0],
                                color[1],
                                color[2],
                            });
                        }
                    }
                }
            }
        }
    }
}

const std::vector<Vertex>& Chunk::mesh_vertices() const noexcept {
    return mesh_vertices_;
}

std::size_t Chunk::visible_face_count() const noexcept {
    return mesh_vertices_.size() / 6U;
}

std::size_t Chunk::index_of(int x, int y, int z) noexcept {
    return (static_cast<std::size_t>(x) * kSize +
            static_cast<std::size_t>(y)) *
               kSize +
           static_cast<std::size_t>(z);
}

}  // namespace convergence
