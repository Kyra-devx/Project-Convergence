#include "convergence/voxel_world.hpp"

namespace convergence {

VoxelWorld::VoxelWorld()
    : blocks_(
          static_cast<std::size_t>(kWidth) *
              static_cast<std::size_t>(kHeight) *
              static_cast<std::size_t>(kDepth),
          kAir) {}

bool VoxelWorld::contains(int x, int y, int z) const noexcept {
    return x >= 0 && x < kWidth &&
           y >= 0 && y < kHeight &&
           z >= 0 && z < kDepth;
}

VoxelId VoxelWorld::get(int x, int y, int z) const noexcept {
    if (!contains(x, y, z)) {
        return kAir;
    }
    return blocks_[index_of(x, y, z)];
}

bool VoxelWorld::set(int x, int y, int z, VoxelId id) noexcept {
    if (!contains(x, y, z)) {
        return false;
    }
    blocks_[index_of(x, y, z)] = id;
    return true;
}

std::size_t VoxelWorld::block_count() const noexcept {
    return blocks_.size();
}

std::size_t VoxelWorld::index_of(int x, int y, int z) noexcept {
    return (static_cast<std::size_t>(y) * kDepth +
            static_cast<std::size_t>(z)) *
               kWidth +
           static_cast<std::size_t>(x);
}

}  // namespace convergence
