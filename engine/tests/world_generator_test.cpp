#include "WorldGenerator.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace {
constexpr int kSize = WorldGenerator::CHUNK_SIZE;
constexpr int kVerticalSections =
    (WorldGenerator::MAX_TERRAIN_HEIGHT + kSize - 1) / kSize;
constexpr std::size_t kBlockCount =
    static_cast<std::size_t>(kSize) * kSize * kSize;
using BlockArray = std::array<std::uint8_t, kBlockCount>;
using WorldSlices = std::array<BlockArray, kVerticalSections>;

std::size_t index_of(int x, int y, int z) {
    return (static_cast<std::size_t>(x) * kSize + y) * kSize + z;
}
WorldSlices generate_column(const WorldGenerator& generator) {
    WorldSlices slices{};
    for (int y = 0; y < kVerticalSections; ++y)
        generator.generateChunk(0, y, 0, slices[y].data());
    return slices;
}
std::uint8_t block_at(const WorldSlices& slices, int x, int worldY, int z) {
    return slices[worldY / kSize][index_of(x, worldY % kSize, z)];
}
}  // namespace

int main() {
    const WorldGenerator first(12345);
    const WorldGenerator sameSeed(12345);
    const WorldGenerator anotherSeed(987654321);
    const WorldSlices firstSlices = generate_column(first);
    const WorldSlices sameSeedSlices = generate_column(sameSeed);
    assert(firstSlices == sameSeedSlices);

    for (int x = 0; x < kSize; ++x) {
        for (int z = 0; z < kSize; ++z) {
            const int height = static_cast<int>(
                first.getTerrainHeight(static_cast<float>(x), static_cast<float>(z)));
            assert(height >= 1 && height <= 127);
            assert(height - 1 < kVerticalSections * kSize);
            const std::uint8_t top =
                (height < WorldGenerator::SEA_LEVEL + 2) ? 2U : 1U;
            assert(block_at(firstSlices, x, height - 1, z) == top);
            assert(block_at(firstSlices, x, height - 2, z) == 2U);
            assert(block_at(firstSlices, x, height - 5, z) == 3U);
        }
    }

    bool changed = false;
    for (int x = -1024; x <= 1024 && !changed; x += 29) {
        for (int z = -1024; z <= 1024; z += 31) {
            if (first.getTerrainHeight(static_cast<float>(x), static_cast<float>(z)) !=
                anotherSeed.getTerrainHeight(static_cast<float>(x), static_cast<float>(z))) {
                changed = true;
                break;
            }
        }
    }
    assert(changed);

    BlockArray above{};
    first.generateChunk(0, kVerticalSections, 0, above.data());
    for (const std::uint8_t block : above) assert(block == 0U);
    first.generateChunk(0, 0, 0, nullptr);
    return 0;
}
