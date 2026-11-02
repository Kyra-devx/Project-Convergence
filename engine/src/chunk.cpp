#include "convergence/chunk.hpp"

#include <array>
#include <utility>

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
    // -Z
    {{{{1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, 0.0F}},
      {{1.0F, 1.0F, 0.0F}}}},
    // +Z
    {{{{0.0F, 0.0F, 1.0F}},
      {{1.0F, 0.0F, 1.0F}},
      {{1.0F, 1.0F, 1.0F}},
      {{0.0F, 1.0F, 1.0F}}}},
}};

[[nodiscard]] Vec3 color_for(std::uint8_t block, std::size_t face) noexcept {
    switch (block) {
        case Chunk::kGrass:
            if (face == 2) return {{0.3F, 0.7F, 0.2F}};
            if (face == 3) return {{0.4F, 0.25F, 0.1F}};
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
    mesh_vertices_.reserve(6U * kSizeX * kSizeZ * 6U);
}

Chunk::Chunk(ChunkData&& input)
    : data_(std::move(input)) {
    mesh_vertices_.reserve(6U * kSizeX * kSizeZ * 6U);
}

int Chunk::chunk_x() const noexcept { return data_.chunk_x; }
int Chunk::chunk_z() const noexcept { return data_.chunk_z; }

bool Chunk::contains(int x, int y, int z) const noexcept {
    return x >= 0 && x < kSizeX &&
           y >= 0 && y < kSizeY &&
           z >= 0 && z < kSizeZ;
}

std::uint8_t Chunk::block_at(int x, int y, int z) const noexcept {
    if (!contains(x, y, z)) return kAir;
    return data_.blocks[index_of(x, y, z)];
}

bool Chunk::set_block(int x, int y, int z,
                      std::uint8_t block) noexcept {
    if (!contains(x, y, z)) return false;
    data_.blocks[index_of(x, y, z)] = block;
    mesh_dirty_ = true;
    return true;
}

void Chunk::mark_dirty() noexcept { mesh_dirty_ = true; }
bool Chunk::is_dirty() const noexcept { return mesh_dirty_; }

void Chunk::build_mesh(const NeighborLookup& neighbor_lookup) {
    mesh_vertices_.clear();

    constexpr std::array<std::array<std::size_t, 3>, 2> kTriangles{{
        {{0U, 1U, 2U}},
        {{0U, 2U, 3U}},
    }};

    for (int y = 0; y < kSizeY; ++y) {
        for (int z = 0; z < kSizeZ; ++z) {
            for (int x = 0; x < kSizeX; ++x) {
                const std::uint8_t block = block_at(x, y, z);
                if (block == kAir) continue;

                for (std::size_t face = 0; face < kFaceNormals.size(); ++face) {
                    const int nx = x + kNeighborOffsets[face][0];
                    const int ny = y + kNeighborOffsets[face][1];
                    const int nz = z + kNeighborOffsets[face][2];

                    std::uint8_t neighbor = kAir;
                    if (contains(nx, ny, nz)) {
                        neighbor = block_at(nx, ny, nz);
                    } else if (neighbor_lookup) {
                        neighbor = neighbor_lookup(nx, ny, nz);
                    }
                    if (neighbor != kAir) continue;

                    const Vec3 color = color_for(block, face);
                    const Vec3 normal = kFaceNormals[face];
                    const FaceCorners& corners = kFaceCorners[face];

                    for (const auto& triangle : kTriangles) {
                        for (const std::size_t cornerIndex : triangle) {
                            const Vec3& corner = corners[cornerIndex];
                            mesh_vertices_.push_back({
                                static_cast<float>(x) + corner[0],
                                static_cast<float>(y) + corner[1],
                                static_cast<float>(z) + corner[2],
                                normal[0], normal[1], normal[2],
                                color[0], color[1], color[2],
                            });
                        }
                    }
                }
            }
        }
    }

    mesh_dirty_ = false;
    ++mesh_revision_;
}

const std::vector<Vertex>& Chunk::mesh_vertices() const noexcept {
    return mesh_vertices_;
}

std::size_t Chunk::visible_face_count() const noexcept {
    return mesh_vertices_.size() / 6U;
}

std::uint64_t Chunk::mesh_revision() const noexcept {
    return mesh_revision_;
}

std::size_t Chunk::index_of(int x, int y, int z) noexcept {
    return cvg::index(x, y, z);
}

}  // namespace convergence
