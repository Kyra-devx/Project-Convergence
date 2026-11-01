#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace convergence {

using VoxelId = std::uint16_t;

class VoxelWorld final {
public:
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 256;
    static constexpr int kDepth = 16;
    static constexpr VoxelId kAir = 0;

    VoxelWorld();

    [[nodiscard]] bool contains(int x, int y, int z) const noexcept;
    [[nodiscard]] VoxelId get(int x, int y, int z) const noexcept;
    [[nodiscard]] bool set(int x, int y, int z, VoxelId id) noexcept;
    [[nodiscard]] std::size_t block_count() const noexcept;

private:
    [[nodiscard]] static std::size_t index_of(int x, int y, int z) noexcept;
    std::vector<VoxelId> blocks_;
};

}  // namespace convergence
