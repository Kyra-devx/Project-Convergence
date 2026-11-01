#pragma once
#include <cstdint>
#include "FastNoiseLite.h"

class WorldGenerator final {
public:
    explicit WorldGenerator(std::int64_t seed);

    // One 16x16x16 section at world chunk coordinates (chunkX, chunkY, chunkZ).
    // Output layout is [localX][localY][localZ], matching convergence::Chunk.
    void generateChunk(int chunkX, int chunkY, int chunkZ,
                       std::uint8_t* blocksOut) const noexcept;
    [[nodiscard]] float getTerrainHeight(float worldX,
                                         float worldZ) const noexcept;

    static constexpr int CHUNK_SIZE = 16;
    static constexpr int SEA_LEVEL = 32;
    static constexpr int BASE_HEIGHT = 40;
    static constexpr int HEIGHT_VARIATION = 24;
    static constexpr int MAX_TERRAIN_HEIGHT =
        BASE_HEIGHT + HEIGHT_VARIATION + 3;

private:
    [[nodiscard]] int calculateHeight(float worldX,
                                      float worldZ) const noexcept;
    FastNoiseLite heightNoise;
    FastNoiseLite detailNoise;
    FastNoiseLite biomeNoise;
};
