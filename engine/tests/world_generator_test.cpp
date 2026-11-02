#include "WorldGenerator.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

int main() {
    const WorldGenerator first(12345);
    const WorldGenerator sameSeed(12345);
    const WorldGenerator anotherSeed(987654321);

    using BlockArray = std::array<std::uint8_t, cvg::BLOCK_COUNT>;
    BlockArray firstBlocks{};
    BlockArray sameSeedBlocks{};
    first.generateChunk(0, 0, firstBlocks.data());
    sameSeed.generateChunk(0, 0, sameSeedBlocks.data());

    // One full 16x128x16 chunk is generated deterministically for one seed.
    assert(firstBlocks == sameSeedBlocks);
    static_assert(cvg::BLOCK_COUNT == 32768U,
                  "A chunk must contain 16x128x16 block IDs.");

    for (int x = 0; x < cvg::SX; ++x) {
        for (int z = 0; z < cvg::SZ; ++z) {
            const int height = static_cast<int>(
                first.getTerrainHeight(static_cast<float>(x),
                                       static_cast<float>(z)));
            assert(height >= 1 && height < cvg::SY);

            const std::uint8_t expectedTop =
                (height < 34) ? 2U : 1U;
            assert(firstBlocks[cvg::index(x, height - 1, z)] == expectedTop);
            assert(firstBlocks[cvg::index(x, height - 2, z)] == 2U);
            assert(firstBlocks[cvg::index(x, height - 5, z)] == 3U);

            for (int y = height; y < cvg::SY; ++y) {
                assert(firstBlocks[cvg::index(x, y, z)] == 0U);
            }
        }
    }

    BlockArray differentSeedBlocks{};
    anotherSeed.generateChunk(0, 0, differentSeedBlocks.data());
    assert(firstBlocks != differentSeedBlocks);

    // Negative chunk coordinates are valid and use the same canonical layout.
    BlockArray negativeBlocks{};
    first.generateChunk(-1, -1, negativeBlocks.data());
    assert(negativeBlocks[cvg::index(0, 20, 0)] == 3U);

    first.generateChunk(0, 0, nullptr);
    return 0;
}
