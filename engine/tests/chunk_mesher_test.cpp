#include "convergence/chunk.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace {

std::array<float, 3> cross(const convergence::Vertex& a,
                           const convergence::Vertex& b,
                           const convergence::Vertex& c) {
    const float ab_x = b.x - a.x;
    const float ab_y = b.y - a.y;
    const float ab_z = b.z - a.z;
    const float ac_x = c.x - a.x;
    const float ac_y = c.y - a.y;
    const float ac_z = c.z - a.z;

    return {{
        ab_y * ac_z - ab_z * ac_y,
        ab_z * ac_x - ab_x * ac_z,
        ab_x * ac_y - ab_y * ac_x,
    }};
}

}  // namespace

int main() {
    using convergence::Chunk;

    // Empty chunk: no geometry.
    Chunk empty;
    empty.build_mesh();
    assert(empty.mesh_vertices().empty());
    assert(empty.visible_face_count() == 0U);

    // One block: six exposed faces, two triangles per face.
    Chunk single;
    assert(single.set_block(4, 4, 4, Chunk::kStone));
    single.build_mesh();
    assert(single.visible_face_count() == 6U);
    assert(single.mesh_vertices().size() == 36U);

    // Every triangle must wind in the same direction as its declared normal.
    const auto& single_vertices = single.mesh_vertices();
    for (std::size_t face = 0; face < 6U; ++face) {
        const std::size_t first = face * 6U;
        const auto winding = cross(
            single_vertices[first],
            single_vertices[first + 1U],
            single_vertices[first + 2U]
        );
        const auto& normal = single_vertices[first];
        const float normal_dot_winding =
            winding[0] * normal.nx +
            winding[1] * normal.ny +
            winding[2] * normal.nz;
        assert(normal_dot_winding > 0.0F);
    }

    // Two adjacent blocks: the shared pair of faces is removed.
    Chunk pair;
    assert(pair.set_block(0, 0, 0, Chunk::kGrass));
    assert(pair.set_block(1, 0, 0, Chunk::kDirt));
    pair.build_mesh();
    assert(pair.visible_face_count() == 10U);
    assert(pair.mesh_vertices().size() == 60U);

    // A completely solid 16^3 chunk exposes only its six outer surfaces.
    Chunk solid;
    for (int x = 0; x < Chunk::kSize; ++x) {
        for (int y = 0; y < Chunk::kSize; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                assert(solid.set_block(x, y, z, Chunk::kStone));
            }
        }
    }
    solid.build_mesh();

    constexpr std::size_t kExpectedVisibleFaces =
        6U * Chunk::kSize * Chunk::kSize;
    assert(solid.visible_face_count() == kExpectedVisibleFaces);
    assert(solid.mesh_vertices().size() == kExpectedVisibleFaces * 6U);

    constexpr std::size_t kNaiveTriangleCount =
        Chunk::kBlockCount * 6U * 2U;
    constexpr std::size_t kCulledTriangleCount =
        kExpectedVisibleFaces * 2U;
    assert(kNaiveTriangleCount == 49152U);
    assert(kCulledTriangleCount == 3072U);

    // Out-of-range writes are rejected; reads outside the chunk are air.
    assert(!single.set_block(-1, 0, 0, Chunk::kStone));
    assert(single.block_at(-1, 0, 0) == Chunk::kAir);

    return 0;
}
