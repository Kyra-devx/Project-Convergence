#include "ChunkManager.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>

namespace {

bool wait_for_world(ChunkManager& manager,
                    std::size_t expectedLoaded,
                    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        manager.update(8.0F, 8.0F);
        if (manager.loaded_chunk_count() == expectedLoaded &&
            manager.pending_generation_count() == 0U &&
            manager.pending_mesh_count() == 0U) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

bool has_positive_x_face_at(const std::vector<ChunkMeshView>& chunks,
                            float x, float minY, float maxY,
                            float minZ, float maxZ) {
    for (const ChunkMeshView& chunk : chunks) {
        // The shared x=16 boundary is the +X side of chunk column (0, 0).
        if (chunk.coord.x != 0 || chunk.coord.z != 0 ||
            chunk.vertices == nullptr) {
            continue;
        }

        const auto& vertices = *chunk.vertices;
        for (std::size_t first = 0;
             first + 5U < vertices.size(); first += 6U) {
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
            if (match) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

int main() {
    const WorldGenerator generator(12345);
    ChunkManager manager(generator, 1);

    assert(manager.worker_running());
    manager.update(8.0F, 8.0F);
    assert(manager.loaded_chunk_count() == 0U);
    assert(manager.pending_generation_count() > 0U);
    assert(manager.pending_generation_count() <=
           ChunkManager::kMaximumQueuedRequests);

    // The world becomes available asynchronously; meshing remains budgeted.
    const bool worldReady =
        wait_for_world(manager, 9U, std::chrono::seconds(10));
    assert(worldReady);
    if (!worldReady) {
        manager.cleanup();
        return 1;
    }
    for (int x = -1; x <= 1; ++x) {
        for (int z = -1; z <= 1; ++z) {
            assert(manager.is_chunk_loaded(x, z));
        }
    }

    // A full 128-block column is generated; y=20 is below the surface.
    assert(manager.get_world_block(-1, 20, -1) == convergence::Chunk::kStone);
    assert(manager.get_world_block(15, 20, 8) == convergence::Chunk::kStone);
    assert(manager.get_world_block(16, 20, 8) == convergence::Chunk::kStone);

    const auto meshes = manager.chunk_meshes();
    // The shared x=16 plane has no +X face after both chunks are meshed.
    assert(!has_positive_x_face_at(meshes, 16.0F, 20.0F, 29.0F, 8.0F, 9.0F));

    // Editing a block at the +X chunk boundary invalidates both loaded sides.
    const bool firstEdit =
        manager.set_world_block(15, 20, 8, convergence::Chunk::kGrass);
    assert(firstEdit);
    if (!firstEdit) return 1;
    assert(manager.get_world_block(15, 20, 8) == convergence::Chunk::kGrass);
    assert(manager.pending_mesh_count() == 2U);
    const bool repeatedEdit =
        manager.set_world_block(15, 20, 8, convergence::Chunk::kGrass);
    assert(repeatedEdit);
    assert(manager.pending_mesh_count() == 2U);  // Idempotent write.
    manager.update(8.0F, 8.0F);
    assert(manager.pending_mesh_count() == 0U);
    const bool invalidY =
        manager.set_world_block(15, -1, 8, convergence::Chunk::kStone);
    const bool unloadedChunk =
        manager.set_world_block(1000, 20, 8, convergence::Chunk::kStone);
    assert(!invalidY);
    assert(!unloadedChunk);

    const std::uint64_t revisionBeforeMove = manager.mesh_revision();
    manager.update(160.0F, 8.0F);  // Camera enters chunk X=10.
    assert(manager.mesh_revision() > revisionBeforeMove);
    assert(!manager.is_chunk_loaded(0, 0));
    assert(manager.loaded_chunk_count() == 0U);

    // Requests from a previous camera region are pruned and the worker stops.
    manager.cleanup();
    assert(!manager.worker_running());
    assert(manager.loaded_chunk_count() == 0U);
    assert(manager.pending_generation_count() == 0U);
    assert(manager.chunk_meshes().empty());
    manager.update(8.0F, 8.0F);  // A cleaned manager remains stopped.
    assert(manager.loaded_chunk_count() == 0U);

    // Negative coordinates use mathematical floor division.
    ChunkManager negativeManager(generator, 0);
    negativeManager.update(-0.5F, -0.5F);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((!negativeManager.is_chunk_loaded(-1, -1) ||
            negativeManager.pending_generation_count() != 0U ||
            negativeManager.pending_mesh_count() != 0U) &&
           std::chrono::steady_clock::now() < deadline) {
        negativeManager.update(-0.5F, -0.5F);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(negativeManager.is_chunk_loaded(-1, -1));
    assert(negativeManager.pending_generation_count() == 0U);
    assert(negativeManager.pending_mesh_count() == 0U);
    assert(negativeManager.get_world_block(-1, 20, -1) ==
           convergence::Chunk::kStone);
    const bool negativeEdit = negativeManager.set_world_block(
        -1, 20, -1, convergence::Chunk::kGrass);
    assert(negativeEdit);
    if (!negativeEdit) return 1;
    assert(negativeManager.get_world_block(-1, 20, -1) ==
           convergence::Chunk::kGrass);
    assert(negativeManager.pending_mesh_count() == 1U);
    negativeManager.cleanup();

    // Non-finite camera input is ignored.
    ChunkManager invalidManager(generator, 0);
    invalidManager.update(std::numeric_limits<float>::quiet_NaN(), 0.0F);
    assert(invalidManager.loaded_chunk_count() == 0U);
    invalidManager.cleanup();
    return 0;
}
