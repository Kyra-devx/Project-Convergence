#pragma once

#include "VoxelDims.h"
#include "FastNoiseLite.h"

#include <cstdint>

class WorldGenerator final {
public:
    explicit WorldGenerator(std::int64_t seed);

    // Generate one complete 16x128x16 X/Z chunk column.
    // blocksOut uses cvg::index(x, y, z); output order is X-fastest.
    void generateChunk(int chunkX, int chunkZ,
                       std::uint8_t* blocksOut) const noexcept;

    [[nodiscard]] float getTerrainHeight(float worldX,
                                         float worldZ) const noexcept;

private:
    [[nodiscard]] int calculateHeight(float worldX,
                                      float worldZ) const noexcept;

    FastNoiseLite heightNoise;
    FastNoiseLite detailNoise;
    FastNoiseLite biomeNoise;

    static constexpr int SEA_LEVEL = 32;
    static constexpr int BASE_HEIGHT = 40;
    static constexpr int HEIGHT_VARIATION = 24;
};
