#include "convergence/voxel_world.hpp"

#include <cassert>

int main() {
    convergence::VoxelWorld world;

    assert(world.block_count() == 16U * 256U * 16U);
    assert(world.get(0, 0, 0) == convergence::VoxelWorld::kAir);

    assert(world.set(2, 10, 3, 7));
    assert(world.get(2, 10, 3) == 7);

    assert(!world.contains(-1, 0, 0));
    assert(!world.set(-1, 0, 0, 9));
    assert(world.get(-1, 0, 0) == convergence::VoxelWorld::kAir);

    assert(!world.contains(16, 0, 0));
    assert(!world.contains(0, 256, 0));
    assert(!world.contains(0, 0, 16));

    return 0;
}
