#include "ChunkManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <unordered_set>
#include <utility>

namespace {

constexpr int kChunkSize = convergence::Chunk::kSize;
constexpr int kUnloadHysteresis = 2;

void append_neighbor(std::vector<ChunkCoord>& output,
                     std::int64_t x, std::int64_t z) {
    if (x < std::numeric_limits<int>::min() ||
        x > std::numeric_limits<int>::max() ||
        z < std::numeric_limits<int>::min() ||
        z > std::numeric_limits<int>::max()) {
        return;
    }
    output.push_back({static_cast<int>(x), static_cast<int>(z)});
}

[[nodiscard]] std::size_t hash_combine(std::size_t seed,
                                       std::size_t value) noexcept {
    return seed ^ (value + static_cast<std::size_t>(0x9e3779b9U) +
                   (seed << 6U) + (seed >> 2U));
}

}  // namespace

std::size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const noexcept {
    const std::size_t xHash = std::hash<int>{}(coord.x);
    const std::size_t zHash = std::hash<int>{}(coord.z);
    return hash_combine(xHash, zHash);
}

ChunkManager::ChunkManager(const WorldGenerator& generator,
                           int viewDistance,
                           std::size_t maxLoadsPerUpdate)
    : generator_(generator),
      viewDistance_(std::clamp(viewDistance, 0, kMaximumViewDistance)),
      maxLoadsPerUpdate_(std::max<std::size_t>(1U, maxLoadsPerUpdate)) {}

ChunkManager::~ChunkManager() {
    cleanup();
}

std::int64_t ChunkManager::floor_div(std::int64_t value,
                                     std::int64_t divisor) noexcept {
    const std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

void ChunkManager::update(float cameraWorldX, float cameraWorldZ) {
    if (!std::isfinite(cameraWorldX) || !std::isfinite(cameraWorldZ)) {
        return;
    }

    const double cameraChunkX =
        std::floor(static_cast<double>(cameraWorldX) / kChunkSize);
    const double cameraChunkZ =
        std::floor(static_cast<double>(cameraWorldZ) / kChunkSize);
    if (cameraChunkX < std::numeric_limits<int>::min() ||
        cameraChunkX > std::numeric_limits<int>::max() ||
        cameraChunkZ < std::numeric_limits<int>::min() ||
        cameraChunkZ > std::numeric_limits<int>::max()) {
        return;
    }

    const int cameraCX = static_cast<int>(cameraChunkX);
    const int cameraCZ = static_cast<int>(cameraChunkZ);
    std::vector<ChunkCoord> columnsToRemesh;
    std::vector<ChunkCoord> loadedColumns;

    unload_distant_chunks(cameraCX, cameraCZ, columnsToRemesh);
    load_chunks_around(cameraCX, cameraCZ, loadedColumns);

    for (const ChunkCoord& loaded : loadedColumns) {
        columnsToRemesh.push_back(loaded);
        append_neighbor(columnsToRemesh, static_cast<std::int64_t>(loaded.x) - 1, loaded.z);
        append_neighbor(columnsToRemesh, static_cast<std::int64_t>(loaded.x) + 1, loaded.z);
        append_neighbor(columnsToRemesh, loaded.x, static_cast<std::int64_t>(loaded.z) - 1);
        append_neighbor(columnsToRemesh, loaded.x, static_cast<std::int64_t>(loaded.z) + 1);
    }

    if (!columnsToRemesh.empty()) {
        rebuild_meshes_around(columnsToRemesh);
        combined_mesh_dirty_ = true;
        ++mesh_revision_;
    }
}

void ChunkManager::cleanup() noexcept {
    chunks_.clear();
    combined_mesh_.clear();
    combined_mesh_dirty_ = false;
    ++mesh_revision_;
}

bool ChunkManager::is_chunk_loaded(int chunkX, int chunkZ) const noexcept {
    return chunks_.find({chunkX, chunkZ}) != chunks_.end();
}

std::size_t ChunkManager::loaded_chunk_count() const noexcept {
    return chunks_.size();
}

std::uint8_t ChunkManager::get_world_block(int worldX, int worldY,
                                           int worldZ) const noexcept {
    return get_world_block64(worldX, worldY, worldZ);
}

const std::vector<convergence::Vertex>& ChunkManager::mesh_vertices() const {
    if (combined_mesh_dirty_) {
        rebuild_combined_mesh();
    }
    return combined_mesh_;
}

std::uint64_t ChunkManager::mesh_revision() const noexcept {
    return mesh_revision_;
}

int ChunkManager::view_distance() const noexcept {
    return viewDistance_;
}

void ChunkManager::unload_distant_chunks(
    int cameraChunkX, int cameraChunkZ,
    std::vector<ChunkCoord>& columnsToRemesh) {
    const std::int64_t threshold =
        static_cast<std::int64_t>(viewDistance_) + kUnloadHysteresis;

    for (auto it = chunks_.begin(); it != chunks_.end();) {
        const std::int64_t dx =
            std::llabs(static_cast<std::int64_t>(it->first.x) - cameraChunkX);
        const std::int64_t dz =
            std::llabs(static_cast<std::int64_t>(it->first.z) - cameraChunkZ);
        if (dx > threshold || dz > threshold) {
            const ChunkCoord removed = it->first;
            append_neighbor(columnsToRemesh, static_cast<std::int64_t>(removed.x) - 1, removed.z);
            append_neighbor(columnsToRemesh, static_cast<std::int64_t>(removed.x) + 1, removed.z);
            append_neighbor(columnsToRemesh, removed.x, static_cast<std::int64_t>(removed.z) - 1);
            append_neighbor(columnsToRemesh, removed.x, static_cast<std::int64_t>(removed.z) + 1);
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

void ChunkManager::load_chunks_around(
    int cameraChunkX, int cameraChunkZ,
    std::vector<ChunkCoord>& loadedColumns) {
    struct Candidate {
        ChunkCoord coord;
        std::int64_t distanceSquared;
    };

    std::vector<Candidate> candidates;
    const std::int64_t minX =
        static_cast<std::int64_t>(cameraChunkX) - viewDistance_;
    const std::int64_t maxX =
        static_cast<std::int64_t>(cameraChunkX) + viewDistance_;
    const std::int64_t minZ =
        static_cast<std::int64_t>(cameraChunkZ) - viewDistance_;
    const std::int64_t maxZ =
        static_cast<std::int64_t>(cameraChunkZ) + viewDistance_;

    for (std::int64_t x = minX; x <= maxX; ++x) {
        for (std::int64_t z = minZ; z <= maxZ; ++z) {
            if (x < std::numeric_limits<int>::min() ||
                x > std::numeric_limits<int>::max() ||
                z < std::numeric_limits<int>::min() ||
                z > std::numeric_limits<int>::max()) {
                continue;
            }

            const ChunkCoord coord{
                static_cast<int>(x), static_cast<int>(z)};
            if (chunks_.find(coord) != chunks_.end()) {
                continue;
            }

            const std::int64_t dx = x - cameraChunkX;
            const std::int64_t dz = z - cameraChunkZ;
            candidates.push_back({coord, dx * dx + dz * dz});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& lhs, const Candidate& rhs) {
                  if (lhs.distanceSquared != rhs.distanceSquared) {
                      return lhs.distanceSquared < rhs.distanceSquared;
                  }
                  if (lhs.coord.x != rhs.coord.x) {
                      return lhs.coord.x < rhs.coord.x;
                  }
                  return lhs.coord.z < rhs.coord.z;
              });

    const std::size_t loadCount =
        std::min(maxLoadsPerUpdate_, candidates.size());
    std::array<std::uint8_t, convergence::Chunk::kBlockCount> blocks{};

    for (std::size_t candidateIndex = 0;
         candidateIndex < loadCount; ++candidateIndex) {
        const ChunkCoord coord = candidates[candidateIndex].coord;
        auto column = std::make_unique<ChunkColumn>();
        auto inserted = chunks_.emplace(coord, std::move(column));
        if (!inserted.second) {
            continue;
        }

        ChunkColumn& loaded = *inserted.first->second;
        for (int sectionY = 0;
             sectionY < kVerticalSectionCount; ++sectionY) {
            generator_.generateChunk(
                coord.x, sectionY, coord.z, blocks.data());
            convergence::Chunk& section = loaded.sections[
                static_cast<std::size_t>(sectionY)];

            for (int x = 0; x < kChunkSize; ++x) {
                for (int y = 0; y < kChunkSize; ++y) {
                    for (int z = 0; z < kChunkSize; ++z) {
                        const std::size_t index =
                            (static_cast<std::size_t>(x) * kChunkSize + y) *
                                kChunkSize + z;
                        const std::uint8_t block = blocks[index];
                        if (block != convergence::Chunk::kAir) {
                            (void)section.set_block(x, y, z, block);
                        }
                    }
                }
            }
        }

        loadedColumns.push_back(coord);
    }
}

void ChunkManager::rebuild_meshes_around(
    const std::vector<ChunkCoord>& columnsToRemesh) {
    std::unordered_set<ChunkCoord, ChunkCoordHash> unique;
    unique.reserve(columnsToRemesh.size());
    for (const ChunkCoord& coord : columnsToRemesh) {
        if (chunks_.find(coord) != chunks_.end()) {
            unique.insert(coord);
        }
    }

    std::vector<ChunkCoord> ordered(unique.begin(), unique.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const ChunkCoord& lhs, const ChunkCoord& rhs) {
                  return lhs.x == rhs.x ? lhs.z < rhs.z : lhs.x < rhs.x;
              });

    for (const ChunkCoord& coord : ordered) {
        rebuild_column_mesh(coord.x, coord.z);
    }
}

void ChunkManager::rebuild_column_mesh(int chunkX, int chunkZ) {
    const auto found = chunks_.find({chunkX, chunkZ});
    if (found == chunks_.end()) {
        return;
    }

    ChunkColumn& column = *found->second;
    column.mesh_vertices.clear();

    for (int sectionY = 0;
         sectionY < kVerticalSectionCount; ++sectionY) {
        convergence::Chunk& section = column.sections[
            static_cast<std::size_t>(sectionY)];

        const auto neighborLookup =
            [this, chunkX, chunkZ, sectionY](
                int localX, int localY, int localZ) -> std::uint8_t {
                const std::int64_t worldX =
                    static_cast<std::int64_t>(chunkX) * kChunkSize + localX;
                const std::int64_t worldY =
                    static_cast<std::int64_t>(sectionY) * kChunkSize + localY;
                const std::int64_t worldZ =
                    static_cast<std::int64_t>(chunkZ) * kChunkSize + localZ;
                return get_world_block64(worldX, worldY, worldZ);
            };

        section.build_mesh(neighborLookup);
        const auto& sectionVertices = section.mesh_vertices();
        column.mesh_vertices.reserve(
            column.mesh_vertices.size() + sectionVertices.size());

        for (const convergence::Vertex& localVertex : sectionVertices) {
            convergence::Vertex worldVertex = localVertex;
            worldVertex.x += static_cast<float>(
                static_cast<std::int64_t>(chunkX) * kChunkSize);
            worldVertex.y += static_cast<float>(sectionY * kChunkSize);
            worldVertex.z += static_cast<float>(
                static_cast<std::int64_t>(chunkZ) * kChunkSize);
            column.mesh_vertices.push_back(worldVertex);
        }
    }
}

std::uint8_t ChunkManager::get_world_block64(
    std::int64_t worldX, std::int64_t worldY,
    std::int64_t worldZ) const noexcept {
    if (worldY < 0 ||
        worldY >= static_cast<std::int64_t>(kVerticalSectionCount) * kChunkSize) {
        return convergence::Chunk::kAir;
    }

    const std::int64_t chunkX64 = floor_div(worldX, kChunkSize);
    const std::int64_t chunkZ64 = floor_div(worldZ, kChunkSize);
    if (chunkX64 < std::numeric_limits<int>::min() ||
        chunkX64 > std::numeric_limits<int>::max() ||
        chunkZ64 < std::numeric_limits<int>::min() ||
        chunkZ64 > std::numeric_limits<int>::max()) {
        return convergence::Chunk::kAir;
    }

    const int chunkX = static_cast<int>(chunkX64);
    const int chunkZ = static_cast<int>(chunkZ64);
    const int localX = static_cast<int>(worldX - chunkX64 * kChunkSize);
    const int localZ = static_cast<int>(worldZ - chunkZ64 * kChunkSize);
    const int sectionY = static_cast<int>(worldY / kChunkSize);
    const int localY = static_cast<int>(worldY % kChunkSize);

    const auto found = chunks_.find({chunkX, chunkZ});
    if (found == chunks_.end()) {
        return convergence::Chunk::kAir;
    }

    return found->second->sections[static_cast<std::size_t>(sectionY)]
        .block_at(localX, localY, localZ);
}

void ChunkManager::rebuild_combined_mesh() const {
    combined_mesh_.clear();

    std::size_t totalVertices = 0U;
    for (const auto& entry : chunks_) {
        totalVertices += entry.second->mesh_vertices.size();
    }

    combined_mesh_.reserve(totalVertices);
    // Iterate the coordinate map order for deterministic mesh ordering.
    std::vector<std::pair<ChunkCoord, const ChunkColumn*>> coordinateOrder;
    coordinateOrder.reserve(chunks_.size());
    for (const auto& entry : chunks_) {
        coordinateOrder.push_back({entry.first, entry.second.get()});
    }
    std::sort(coordinateOrder.begin(), coordinateOrder.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.first.x == rhs.first.x
                      ? lhs.first.z < rhs.first.z
                      : lhs.first.x < rhs.first.x;
              });

    for (const auto& entry : coordinateOrder) {
        const auto& vertices = entry.second->mesh_vertices;
        combined_mesh_.insert(
            combined_mesh_.end(), vertices.begin(), vertices.end());
    }

    combined_mesh_dirty_ = false;
}
