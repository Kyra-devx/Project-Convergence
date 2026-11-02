#pragma once

#include <cstddef>

namespace cvg {

inline constexpr int SX = 16;
inline constexpr int SY = 128;
inline constexpr int SZ = 16;
inline constexpr std::size_t BLOCK_COUNT =
    static_cast<std::size_t>(SX) * SY * SZ;

// X is contiguous, then Z, then Y. This is the canonical block-data layout
// used by WorldGenerator, ChunkData, and the native mesher.
[[nodiscard]] constexpr std::size_t index(int x, int y, int z) noexcept {
    return (static_cast<std::size_t>(y) * SZ +
            static_cast<std::size_t>(z)) * SX +
           static_cast<std::size_t>(x);
}

}  // namespace cvg
