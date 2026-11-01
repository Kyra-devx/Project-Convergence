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

    Chunk empty;
    empty.build_mesh();
    assert(empty.mesh_vertices().empty());
    assert(empty.visible_face_count() == 0U);

    Chunk single;
    assert(single.set_block(4, 4, 4, Chunk::kStone));
    single.build_mesh();
    assert(single.visible_face_count() == 6U);
    assert(single.mesh_vertices().size() == 36U);

    const auto& singleVertices = single.mesh_vertices();
    for (std::size_t face = 0; face < 6U; ++face) {
        const std::size_t first = face * 6U;
        const auto winding = cross(
            singleVertices[first], singleVertices[first + 1U],
            singleVertices[first + 2U]);
        const auto& normal = singleVertices[first];
        const float dot =
            winding[0] * normal.nx +
            winding[1] * normal.ny +
            winding[2] * normal.nz;
        assert(dot > 0.0F);
    }

    Chunk pair;
    assert(pair.set_block(0, 0, 0, Chunk::kGrass));
    assert(pair.set_block(1, 0, 0, Chunk::kDirt));
    pair.build_mesh();
    assert(pair.visible_face_count() == 10U);
    assert(pair.mesh_vertices().size() == 60U);

    Chunk solid;
    for (int x = 0; x < Chunk::kSize; ++x) {
        for (int y = 0; y < Chunk::kSize; ++y) {
            for (int z = 0; z < Chunk::kSize; ++z) {
                assert(solid.set_block(x, y, z, Chunk::kStone));
            }
        }
    }
    solid.build_mesh();
    constexpr std::size_t kExpectedFaces =
        6U * Chunk::kSize * Chunk::kSize;
    assert(solid.visible_face_count() == kExpectedFaces);
    assert(solid.mesh_vertices().size() == kExpectedFaces * 6U);
    assert(Chunk::kBlockCount * 6U * 2U == 49152U);
    assert(kExpectedFaces * 2U == 3072U);

    // A neighbor at +X suppresses only the shared boundary face.
    Chunk boundary;
    assert(boundary.set_block(15, 4, 8, Chunk::kStone));
    boundary.build_mesh(
        [](int x, int y, int z) -> std::uint8_t {
            return (x == 16 && y == 4 && z == 8)
                ? Chunk::kStone
                : Chunk::kAir;
        });
    assert(boundary.visible_face_count() == 5U);

    // A missing neighbor remains air, preserving edge visibility while streaming.
    Chunk edge;
    assert(edge.set_block(0, 4, 8, Chunk::kStone));
    edge.build_mesh();
    assert(edge.visible_face_count() == 6U);

    assert(!single.set_block(-1, 0, 0, Chunk::kStone));
    assert(single.block_at(-1, 0, 0) == Chunk::kAir);
    return 0;
}
