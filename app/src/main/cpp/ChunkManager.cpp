#include "ChunkManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <utility>

namespace {

constexpr int kChunkSizeX = cvg::SX;
constexpr int kChunkSizeZ = cvg::SZ;

[[nodiscard]] std::size_t hash_combine(std::size_t seed,
                                       std::size_t value) noexcept {
    return seed ^ (value + static_cast<std::size_t>(0x9e3779b9U) +
                   (seed << 6U) + (seed >> 2U));
}

}  // namespace

std::size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const noexcept {
    return hash_combine(std::hash<int>{}(coord.x),
                        std::hash<int>{}(coord.z));
}

ChunkManager::ChunkManager(const WorldGenerator& generator, int viewDistance)
    : generator_(generator),
      viewDistance_(std::clamp(viewDistance, 0, kMaximumViewDistance)) {
    // Start only after every queue, mutex, and condition-variable member has
    // completed construction; the worker immediately waits on queueCv_.
    worker_ = std::thread(&ChunkManager::worker_loop, this);
}

ChunkManager::~ChunkManager() {
    cleanup();
}

std::int64_t ChunkManager::floor_div(std::int64_t value,
                                     std::int64_t divisor) noexcept {
    const std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

bool ChunkManager::within_unload_range(
    int chunkX, int chunkZ, int cameraChunkX, int cameraChunkZ
) const noexcept {
    const std::int64_t threshold =
        static_cast<std::int64_t>(viewDistance_) + kUnloadHysteresis;
    const std::int64_t dx =
        std::llabs(static_cast<std::int64_t>(chunkX) - cameraChunkX);
    const std::int64_t dz =
        std::llabs(static_cast<std::int64_t>(chunkZ) - cameraChunkZ);
    return dx <= threshold && dz <= threshold;
}

void ChunkManager::update(float cameraWorldX, float cameraWorldZ) {
    if (cleanedUp_ || !std::isfinite(cameraWorldX) ||
        !std::isfinite(cameraWorldZ)) {
        return;
    }

    const double cameraChunkX =
        std::floor(static_cast<double>(cameraWorldX) / kChunkSizeX);
    const double cameraChunkZ =
        std::floor(static_cast<double>(cameraWorldZ) / kChunkSizeZ);
    if (cameraChunkX < std::numeric_limits<int>::min() ||
        cameraChunkX > std::numeric_limits<int>::max() ||
        cameraChunkZ < std::numeric_limits<int>::min() ||
        cameraChunkZ > std::numeric_limits<int>::max()) {
        return;
    }

    const int cameraCX = static_cast<int>(cameraChunkX);
    const int cameraCZ = static_cast<int>(cameraChunkZ);
    bool topologyChanged = false;

    prune_stale_requests(cameraCX, cameraCZ);
    unload_distant_chunks(cameraCX, cameraCZ, topologyChanged);
    collect_ready_chunks(cameraCX, cameraCZ, topologyChanged);
    enqueue_missing_chunks(cameraCX, cameraCZ);

    const bool meshChanged = build_dirty_meshes();
    if (topologyChanged || meshChanged) {
        ++meshRevision_;
    }
}

void ChunkManager::cleanup() noexcept {
    if (cleanedUp_) return;
    cleanedUp_ = true;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stopping_ = true;
        pendingQueue_.clear();
        readyQueue_.clear();
        pendingSet_.clear();
    }
    queueCv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }

    chunks_.clear();
    dirtyQueue_.clear();
    dirtySet_.clear();
    ++meshRevision_;
}

const convergence::Chunk* ChunkManager::getChunk(
    int chunkX, int chunkZ
) const noexcept {
    const auto found = chunks_.find({chunkX, chunkZ});
    return found == chunks_.end() ? nullptr : found->second.get();
}

bool ChunkManager::is_chunk_loaded(int chunkX, int chunkZ) const noexcept {
    return getChunk(chunkX, chunkZ) != nullptr;
}

std::size_t ChunkManager::loaded_chunk_count() const noexcept {
    return chunks_.size();
}

std::size_t ChunkManager::pending_generation_count() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return pendingSet_.size();
}

std::size_t ChunkManager::pending_mesh_count() const noexcept {
    return dirtySet_.size();
}

bool ChunkManager::worker_running() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !stopping_ && worker_.joinable();
}

std::uint8_t ChunkManager::get_world_block(
    int worldX, int worldY, int worldZ
) const noexcept {
    return get_world_block64(worldX, worldY, worldZ);
}

std::vector<ChunkMeshView> ChunkManager::chunk_meshes() const {
    std::vector<ChunkMeshView> views;
    views.reserve(chunks_.size());
    for (const auto& entry : chunks_) {
        views.push_back({
            entry.first,
            &entry.second->mesh_vertices(),
            entry.second->mesh_revision()
        });
    }
    std::sort(views.begin(), views.end(),
              [](const ChunkMeshView& lhs, const ChunkMeshView& rhs) {
                  return lhs.coord.x == rhs.coord.x
                      ? lhs.coord.z < rhs.coord.z
                      : lhs.coord.x < rhs.coord.x;
              });
    return views;
}

std::uint64_t ChunkManager::mesh_revision() const noexcept {
    return meshRevision_;
}

int ChunkManager::view_distance() const noexcept {
    return viewDistance_;
}

void ChunkManager::worker_loop() noexcept {
    for (;;) {
        ChunkCoord coord{};
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] {
                return stopping_ || !pendingQueue_.empty();
            });

            if (stopping_) return;

            coord = pendingQueue_.front();
            pendingQueue_.pop_front();
            // Keep coord in pendingSet_ while in flight; this prevents
            // duplicate work from being queued before its result is collected.
        }

        try {
            convergence::ChunkData data{};
            data.chunk_x = coord.x;
            data.chunk_z = coord.z;
            generator_.generateChunk(coord.x, coord.z, data.blocks.data());

            std::lock_guard<std::mutex> lock(queueMutex_);
            if (stopping_) return;
            readyQueue_.push_back(std::move(data));
        } catch (...) {
            // A failed allocation/generation request is made retryable. Never
            // let an exception escape the worker thread and terminate the app.
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingSet_.erase(coord);
        }
    }
}

void ChunkManager::prune_stale_requests(
    int cameraChunkX, int cameraChunkZ) {
    std::lock_guard<std::mutex> lock(queueMutex_);

    for (auto it = pendingQueue_.begin(); it != pendingQueue_.end();) {
        if (!within_unload_range(it->x, it->z, cameraChunkX, cameraChunkZ)) {
            pendingSet_.erase(*it);
            it = pendingQueue_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = readyQueue_.begin(); it != readyQueue_.end();) {
        if (!within_unload_range(it->chunk_x, it->chunk_z,
                                 cameraChunkX, cameraChunkZ)) {
            pendingSet_.erase({it->chunk_x, it->chunk_z});
            it = readyQueue_.erase(it);
        } else {
            ++it;
        }
    }
}

void ChunkManager::unload_distant_chunks(
    int cameraChunkX, int cameraChunkZ, bool& topologyChanged) {
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        if (!within_unload_range(it->first.x, it->first.z,
                                 cameraChunkX, cameraChunkZ)) {
            const ChunkCoord removed = it->first;
            it = chunks_.erase(it);
            mark_neighbors_dirty(removed.x, removed.z);
            topologyChanged = true;
        } else {
            ++it;
        }
    }
}

void ChunkManager::collect_ready_chunks(
    int cameraChunkX, int cameraChunkZ, bool& topologyChanged) {
    std::deque<convergence::ChunkData> completed;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const std::size_t count =
            std::min(kCompletedPerUpdate, readyQueue_.size());
        for (std::size_t i = 0; i < count; ++i) {
            completed.push_back(std::move(readyQueue_.front()));
            pendingSet_.erase({
                completed.back().chunk_x, completed.back().chunk_z});
            readyQueue_.pop_front();
        }
    }

    for (convergence::ChunkData& data : completed) {
        if (!within_unload_range(data.chunk_x, data.chunk_z,
                                 cameraChunkX, cameraChunkZ)) {
            continue;
        }

        const ChunkCoord coord{data.chunk_x, data.chunk_z};
        if (chunks_.find(coord) != chunks_.end()) {
            continue;
        }

        auto chunk = std::make_unique<convergence::Chunk>(std::move(data));
        chunks_.emplace(coord, std::move(chunk));
        mark_neighbors_dirty(coord.x, coord.z);
        topologyChanged = true;
    }
}

void ChunkManager::enqueue_missing_chunks(
    int cameraChunkX, int cameraChunkZ) {
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
            if (chunks_.find(coord) != chunks_.end()) continue;

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

    bool notifyWorker = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (stopping_) return;

        // Re-prioritize queued work around the player's latest position.
        std::sort(pendingQueue_.begin(), pendingQueue_.end(),
                  [cameraChunkX, cameraChunkZ](const ChunkCoord& lhs,
                                               const ChunkCoord& rhs) {
            const std::int64_t lhsDx = static_cast<std::int64_t>(lhs.x) - cameraChunkX;
            const std::int64_t lhsDz = static_cast<std::int64_t>(lhs.z) - cameraChunkZ;
            const std::int64_t rhsDx = static_cast<std::int64_t>(rhs.x) - cameraChunkX;
            const std::int64_t rhsDz = static_cast<std::int64_t>(rhs.z) - cameraChunkZ;
            const std::int64_t lhsDistance = lhsDx * lhsDx + lhsDz * lhsDz;
            const std::int64_t rhsDistance = rhsDx * rhsDx + rhsDz * rhsDz;
            if (lhsDistance != rhsDistance) return lhsDistance < rhsDistance;
            if (lhs.x != rhs.x) return lhs.x < rhs.x;
            return lhs.z < rhs.z;
        });

        const std::size_t queueCapacity =
            kMaximumQueuedRequests > pendingSet_.size()
                ? kMaximumQueuedRequests - pendingSet_.size()
                : 0U;
        const std::size_t requestBudget =
            std::min(kRequestsPerUpdate, queueCapacity);
        std::size_t enqueued = 0U;
        for (const Candidate& candidate : candidates) {
            if (enqueued >= requestBudget) break;
            const ChunkCoord coord = candidate.coord;
            if (pendingSet_.insert(coord).second) {
                pendingQueue_.push_back(coord);
                ++enqueued;
                notifyWorker = true;
            }
        }
    }

    if (notifyWorker) queueCv_.notify_one();
}

void ChunkManager::mark_chunk_dirty(int chunkX, int chunkZ) {
    const ChunkCoord coord{chunkX, chunkZ};
    const auto found = chunks_.find(coord);
    if (found == chunks_.end()) return;

    found->second->mark_dirty();
    if (dirtySet_.insert(coord).second) {
        dirtyQueue_.push_back(coord);
    }
}

void ChunkManager::mark_neighbors_dirty(int chunkX, int chunkZ) {
    mark_chunk_dirty(chunkX, chunkZ);

    const auto mark_if_representable = [this](std::int64_t x,
                                               std::int64_t z) {
        if (x < std::numeric_limits<int>::min() ||
            x > std::numeric_limits<int>::max() ||
            z < std::numeric_limits<int>::min() ||
            z > std::numeric_limits<int>::max()) return;
        mark_chunk_dirty(static_cast<int>(x), static_cast<int>(z));
    };

    mark_if_representable(static_cast<std::int64_t>(chunkX) - 1, chunkZ);
    mark_if_representable(static_cast<std::int64_t>(chunkX) + 1, chunkZ);
    mark_if_representable(chunkX, static_cast<std::int64_t>(chunkZ) - 1);
    mark_if_representable(chunkX, static_cast<std::int64_t>(chunkZ) + 1);
}

bool ChunkManager::build_dirty_meshes() {
    std::size_t built = 0U;
    while (built < kMeshesPerUpdate && !dirtyQueue_.empty()) {
        const ChunkCoord coord = dirtyQueue_.front();
        dirtyQueue_.pop_front();
        dirtySet_.erase(coord);

        const auto found = chunks_.find(coord);
        if (found == chunks_.end() || !found->second->is_dirty()) {
            continue;
        }

        rebuild_column_mesh(coord.x, coord.z);
        ++built;
    }
    return built > 0U;
}

void ChunkManager::rebuild_column_mesh(int chunkX, int chunkZ) {
    const auto found = chunks_.find({chunkX, chunkZ});
    if (found == chunks_.end()) return;

    const auto neighborLookup =
        [this, chunkX, chunkZ](int localX, int y, int localZ) {
            const std::int64_t worldX =
                static_cast<std::int64_t>(chunkX) * kChunkSizeX + localX;
            const std::int64_t worldZ =
                static_cast<std::int64_t>(chunkZ) * kChunkSizeZ + localZ;
            return get_world_block64(worldX, y, worldZ);
        };

    found->second->build_mesh(neighborLookup);
}

std::uint8_t ChunkManager::get_world_block64(
    std::int64_t worldX, std::int64_t worldY,
    std::int64_t worldZ) const noexcept {
    if (worldY < 0 || worldY >= cvg::SY) {
        return convergence::Chunk::kAir;
    }

    const std::int64_t chunkX64 = floor_div(worldX, kChunkSizeX);
    const std::int64_t chunkZ64 = floor_div(worldZ, kChunkSizeZ);
    if (chunkX64 < std::numeric_limits<int>::min() ||
        chunkX64 > std::numeric_limits<int>::max() ||
        chunkZ64 < std::numeric_limits<int>::min() ||
        chunkZ64 > std::numeric_limits<int>::max()) {
        return convergence::Chunk::kAir;
    }

    const int chunkX = static_cast<int>(chunkX64);
    const int chunkZ = static_cast<int>(chunkZ64);
    const int localX = static_cast<int>(worldX - chunkX64 * kChunkSizeX);
    const int localZ = static_cast<int>(worldZ - chunkZ64 * kChunkSizeZ);

    const convergence::Chunk* chunk = getChunk(chunkX, chunkZ);
    return chunk != nullptr
        ? chunk->block_at(localX, static_cast<int>(worldY), localZ)
        : convergence::Chunk::kAir;
}

