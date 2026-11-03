#include "Raycast.h"
#include "ChunkManager.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

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

}  // namespace

int main() {
    const WorldGenerator generator(12345);
    ChunkManager manager(generator, 1);
    const bool ready = wait_for_world(manager, 9U, std::chrono::seconds(10));
    assert(ready);
    if (!ready) {
        manager.cleanup();
        return 1;
    }

    // Isolated blocks sit above the generated terrain surface.
    const bool firstBlock =
        manager.set_world_block(5, 80, 8, convergence::Chunk::kStone);
    const bool boundaryBlock =
        manager.set_world_block(16, 80, 8, convergence::Chunk::kStone);
    assert(firstBlock);
    assert(boundaryBlock);
    if (!firstBlock || !boundaryBlock) {
        manager.cleanup();
        return 1;
    }

    RayHit hit = raycastVoxels(
        {1.5F, 80.5F, 8.5F}, {4.0F, 0.0F, 0.0F}, 5.0F, &manager);
    assert(hit.hit && hit.x == 5 && hit.y == 80 && hit.z == 8);
    assert(hit.nx == -1 && hit.ny == 0 && hit.nz == 0);
    assert(std::abs(hit.distance - 3.5F) < 0.001F);

    hit = raycastVoxels(
        {8.5F, 80.5F, 8.5F}, {-1.0F, 0.0F, 0.0F}, 5.0F, &manager);
    assert(hit.hit && hit.x == 5 && hit.nx == 1);
    assert(std::abs(hit.distance - 2.5F) < 0.001F);

    hit = raycastVoxels(
        {8.5F, 80.5F, 8.5F}, {-1.0F, 0.0F, 0.0F}, 2.4F, &manager);
    assert(!hit.hit);

    // Crossing from chunk X=0 into chunk X=1 is handled in world coordinates.
    hit = raycastVoxels(
        {14.5F, 80.5F, 8.5F}, {1.0F, 0.0F, 0.0F}, 5.0F, &manager);
    assert(hit.hit && hit.x == 16 && hit.nx == -1);

    // A ray starting inside a solid voxel has no entry face for placement.
    hit = raycastVoxels(
        {5.25F, 80.5F, 8.5F}, {1.0F, 0.0F, 0.0F}, 5.0F, &manager);
    assert(hit.hit && hit.distance == 0.0F);
    assert(hit.nx == 0 && hit.ny == 0 && hit.nz == 0);

    hit = raycastVoxels(
        {8.5F, 80.5F, 8.5F}, {0.0F, 1.0F, 0.0F}, 5.0F, &manager);
    assert(!hit.hit);

    hit = raycastVoxels(
        {8.5F, 80.5F, 8.5F}, {0.0F, 0.0F, 0.0F}, 5.0F, &manager);
    assert(!hit.hit);
    hit = raycastVoxels(
        {std::numeric_limits<float>::quiet_NaN(), 80.5F, 8.5F},
        {1.0F, 0.0F, 0.0F}, 5.0F, &manager);
    assert(!hit.hit);

    manager.cleanup();

    // Negative world coordinates use floor division, not truncation.
    ChunkManager negativeManager(generator, 1);
    const auto negativeDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool negativeReady = false;
    while (std::chrono::steady_clock::now() < negativeDeadline) {
        negativeManager.update(-8.0F, -8.0F);
        if (negativeManager.loaded_chunk_count() == 9U &&
            negativeManager.pending_generation_count() == 0U &&
            negativeManager.pending_mesh_count() == 0U) {
            negativeReady = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(negativeReady);
    if (!negativeReady) {
        negativeManager.cleanup();
        return 1;
    }

    const bool negativeEdit = negativeManager.set_world_block(
        -1, 80, -1, convergence::Chunk::kStone);
    assert(negativeEdit);
    if (!negativeEdit) {
        negativeManager.cleanup();
        return 1;
    }
    hit = raycastVoxels(
        {-4.5F, 80.5F, -0.5F}, {1.0F, 0.0F, 0.0F}, 5.0F,
        &negativeManager);
    assert(hit.hit && hit.x == -1 && hit.y == 80 && hit.z == -1);
    assert(hit.nx == -1);

    negativeManager.cleanup();
    return 0;
}
