#include "ChunkManager.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

bool has_positive_x_face_at(const std::vector<convergence::Vertex>& vertices,
                            float x, float minY, float maxY,
                            float minZ, float maxZ) {
    for (std::size_t first = 0; first + 5U < vertices.size(); first += 6U) {
        bool match = true;
        for (std::size_t i = 0; i < 6U; ++i) {
            const auto& vertex = vertices[first + i];
            if (vertex.nx != 1.0F || vertex.x != x ||
                vertex.y < minY || vertex.y > maxY ||
                vertex.z < minZ || vertex.z > maxZ) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

}  // namespace

int main() {
    const WorldGenerator generator(12345);
    ChunkManager manager(generator, 1, 1);

    // The initial load budget is enforced and the nearest chunk is loaded first.
    manager.update(8.0F, 8.0F);
    assert(manager.loaded_chunk_count() == 1U);
    assert(manager.is_chunk_loaded(0, 0));
    assert(!manager.is_chunk_loaded(1, 0));

    // Fill the complete 3x3 view around the current camera chunk.
    for (int frame = 0; frame < 12; ++frame) {
        manager.update(8.0F, 8.0F);
    }
    assert(manager.loaded_chunk_count() == 9U);
    for (int x = -1; x <= 1; ++x) {
        for (int z = -1; z <= 1; ++z) {
            assert(manager.is_chunk_loaded(x, z));
        }
    }

    // Negative world coordinates use floor division, not truncation to zero.
    assert(manager.get_world_block(-1, 20, -1) == convergence::Chunk::kStone);
    assert(manager.get_world_block(15, 20, 8) == convergence::Chunk::kStone);
    assert(manager.get_world_block(16, 20, 8) == convergence::Chunk::kStone);

    const auto& merged = manager.mesh_vertices();
    // A +X face at the shared x=16 plane must be culled for solid lower strata.
    assert(!has_positive_x_face_at(merged, 16.0F, 20.0F, 29.0F, 8.0F, 9.0F));

    const std::uint64_t revisionBeforeMove = manager.mesh_revision();
    manager.update(160.0F, 8.0F);  // Move to chunk X=10.
    assert(manager.mesh_revision() > revisionBeforeMove);
    assert(!manager.is_chunk_loaded(0, 0));
    assert(manager.loaded_chunk_count() <= 1U);

    // Load around negative coordinates and confirm the expected chunk key.
    ChunkManager negativeManager(generator, 0, 1);
    negativeManager.update(-0.5F, -0.5F);
    assert(negativeManager.is_chunk_loaded(-1, -1));

    const std::uint64_t cleanupRevision = negativeManager.mesh_revision();
    negativeManager.cleanup();
    assert(negativeManager.loaded_chunk_count() == 0U);
    assert(negativeManager.mesh_vertices().empty());
    assert(negativeManager.mesh_revision() > cleanupRevision);

    // Non-finite camera input is ignored safely.
    negativeManager.update(std::numeric_limits<float>::quiet_NaN(), 0.0F);
    assert(negativeManager.loaded_chunk_count() == 0U);
    return 0;
}
