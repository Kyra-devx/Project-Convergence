#pragma once

#include "WorldGenerator.h"
#include "convergence/chunk.hpp"

#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ChunkCoord {
    int x = 0;
    int z = 0;

    [[nodiscard]] bool operator==(const ChunkCoord& other) const noexcept {
        return x == other.x && z == other.z;
    }
};

struct ChunkCoordHash {
    [[nodiscard]] std::size_t operator()(const ChunkCoord& coord) const noexcept;
};

struct ChunkMeshView {
    ChunkCoord coord;
    const std::vector<convergence::Vertex>* vertices = nullptr;
    std::uint64_t revision = 0U;
};

class ChunkManager final {
public:
    static constexpr std::size_t kRequestsPerUpdate = 8U;
    static constexpr std::size_t kCompletedPerUpdate = 6U;
    static constexpr std::size_t kMeshesPerUpdate = 2U;
    static constexpr std::size_t kMaximumQueuedRequests = 32U;
    static constexpr int kUnloadHysteresis = 2;
    static constexpr int kMaximumViewDistance = 16;

    explicit ChunkManager(const WorldGenerator& generator,
                          int viewDistance = 4);
    ~ChunkManager();

    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;
    ChunkManager(ChunkManager&&) = delete;
    ChunkManager& operator=(ChunkManager&&) = delete;

    // Call update from the GL/render thread. It collects worker data, performs
    // bounded CPU meshing, and never invokes OpenGL.
    void update(float cameraWorldX, float cameraWorldZ);

    // Stops the worker, joins it, and releases all CPU state. Idempotent.
    void cleanup() noexcept;

    [[nodiscard]] const convergence::Chunk* getChunk(
        int chunkX, int chunkZ
    ) const noexcept;
    [[nodiscard]] bool is_chunk_loaded(int chunkX, int chunkZ) const noexcept;
    [[nodiscard]] std::size_t loaded_chunk_count() const noexcept;
    [[nodiscard]] std::size_t pending_generation_count() const noexcept;
    [[nodiscard]] std::size_t pending_mesh_count() const noexcept;
    [[nodiscard]] bool worker_running() const noexcept;

    // World-space block access; call set_world_block from the GL/render thread.
    [[nodiscard]] std::uint8_t get_world_block(
        int worldX, int worldY, int worldZ
    ) const noexcept;
    [[nodiscard]] bool set_world_block(
        int worldX, int worldY, int worldZ, std::uint8_t block
    );

    // Stable, coordinate-sorted views; pointers remain valid until the next update.
    [[nodiscard]] std::vector<ChunkMeshView> chunk_meshes() const;
    [[nodiscard]] std::uint64_t mesh_revision() const noexcept;
    [[nodiscard]] int view_distance() const noexcept;

private:
    [[nodiscard]] static std::int64_t floor_div(
        std::int64_t value, std::int64_t divisor
    ) noexcept;

    [[nodiscard]] bool within_unload_range(
        int chunkX, int chunkZ, int cameraChunkX, int cameraChunkZ
    ) const noexcept;

    void worker_loop() noexcept;
    void prune_stale_requests(int cameraChunkX, int cameraChunkZ);
    void unload_distant_chunks(int cameraChunkX, int cameraChunkZ,
                               bool& topologyChanged);
    void collect_ready_chunks(int cameraChunkX, int cameraChunkZ,
                              bool& topologyChanged);
    void enqueue_missing_chunks(int cameraChunkX, int cameraChunkZ);
    void mark_neighbors_dirty(int chunkX, int chunkZ);
    void mark_chunk_dirty(int chunkX, int chunkZ);
    bool build_dirty_meshes();
    void rebuild_column_mesh(int chunkX, int chunkZ);
    [[nodiscard]] std::uint8_t get_world_block64(
        std::int64_t worldX, std::int64_t worldY,
        std::int64_t worldZ
    ) const noexcept;

    const WorldGenerator& generator_;
    const int viewDistance_;

    // Worker-owned queue synchronization. pendingSet_ includes queued,
    // in-flight, and completed-but-not-yet-collected coordinates.
    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    bool stopping_ = false;
    std::thread worker_;
    std::deque<ChunkCoord> pendingQueue_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> pendingSet_;
    std::deque<convergence::ChunkData> readyQueue_;

    // These containers are touched only from the GL/render thread.
    std::unordered_map<
        ChunkCoord,
        std::unique_ptr<convergence::Chunk>,
        ChunkCoordHash
    > chunks_;
    std::deque<ChunkCoord> dirtyQueue_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtySet_;

    std::uint64_t meshRevision_ = 0U;
    bool cleanedUp_ = false;
};
