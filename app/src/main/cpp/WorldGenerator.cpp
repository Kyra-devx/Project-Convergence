#include "WorldGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {
int offset_seed(std::int64_t seed, std::uint32_t offset) noexcept {
    const std::uint32_t bits =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) + offset);
    if (bits <= 0x7FFFFFFFU) return static_cast<int>(bits);
    return static_cast<int>(static_cast<std::int64_t>(bits) - 0x100000000LL);
}
}  // namespace

WorldGenerator::WorldGenerator(std::int64_t seed) {
    heightNoise.SetSeed(offset_seed(seed, 0U));
    heightNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    heightNoise.SetFrequency(0.01F);
    heightNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    heightNoise.SetFractalOctaves(4);
    heightNoise.SetFractalLacunarity(2.0F);
    heightNoise.SetFractalGain(0.5F);

    detailNoise.SetSeed(offset_seed(seed, 1U));
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFrequency(0.05F);
    // FastNoiseLite defaults to FractalType_None; FBm activates the two
    // configured detail octaves rather than silently sampling only one.
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(2);

    biomeNoise.SetSeed(offset_seed(seed, 2U));
    biomeNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    biomeNoise.SetFrequency(0.005F);
}

int WorldGenerator::calculateHeight(float worldX,
                                    float worldZ) const noexcept {
    const float h1 = heightNoise.GetNoise(worldX, worldZ);
    const float h2 = detailNoise.GetNoise(worldX, worldZ);
    float normalized = std::clamp((h1 + 1.0F) * 0.5F, 0.0F, 1.0F);
    normalized = std::pow(normalized, 1.3F);
    const int height = static_cast<int>(
        BASE_HEIGHT + normalized * HEIGHT_VARIATION + h2 * 3.0F);
    return std::clamp(height, 1, 127);
}

float WorldGenerator::getTerrainHeight(float worldX,
                                       float worldZ) const noexcept {
    return static_cast<float>(calculateHeight(worldX, worldZ));
}

void WorldGenerator::generateChunk(int chunkX, int chunkY, int chunkZ,
                                   std::uint8_t* blocksOut) const noexcept {
    if (blocksOut == nullptr) return;

    constexpr std::size_t kBlockCount =
        static_cast<std::size_t>(CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE;
    std::fill(blocksOut, blocksOut + kBlockCount, static_cast<std::uint8_t>(0));

    const std::int64_t worldOriginY =
        static_cast<std::int64_t>(chunkY) * CHUNK_SIZE;
    for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
        const float worldX = static_cast<float>(
            static_cast<std::int64_t>(chunkX) * CHUNK_SIZE + localX);
        for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
            const float worldZ = static_cast<float>(
                static_cast<std::int64_t>(chunkZ) * CHUNK_SIZE + localZ);
            const int height = calculateHeight(worldX, worldZ);
            for (int localY = 0; localY < CHUNK_SIZE; ++localY) {
                const std::int64_t worldY = worldOriginY + localY;
                std::uint8_t block = 0;
                if (worldY < height - 4) {
                    block = 3;  // Stone
                } else if (worldY < height - 1) {
                    block = 2;  // Dirt
                } else if (worldY == height - 1) {
                    block = (height < SEA_LEVEL + 2) ? 2 : 1;  // Grass/Dirt
                }
                // Contiguous [x][y][z] layout, identical to engine Chunk.
                const std::size_t index =
                    (static_cast<std::size_t>(localX) * CHUNK_SIZE + localY) *
                        CHUNK_SIZE + localZ;
                blocksOut[index] = block;
            }
        }
    }
}
