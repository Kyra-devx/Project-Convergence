#pragma once

#include "WorldGenerator.h"
#include "convergence/chunk.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
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

class ChunkManager final {
public:
    static constexpr std::size_t kDefaultLoadBudgetPerUpdate = 2U;
    static constexpr int kMaximumViewDistance = 16;
    static constexpr int kVerticalSectionCount =
        (WorldGenerator::MAX_TERRAIN_HEIGHT +
         WorldGenerator::CHUNK_SIZE - 1) / WorldGenerator::CHUNK_SIZE;
    static_assert(WorldGenerator::CHUNK_SIZE == convergence::Chunk::kSize,
                  "World generator and engine chunk dimensions must match.");

    explicit ChunkManager(
        const WorldGenerator& generator,
        int viewDistance = 4,
        std::size_t maxLoadsPerUpdate = kDefaultLoadBudgetPerUpdate
    );
    ~ChunkManager();

    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;
    ChunkManager(ChunkManager&&) = delete;
    ChunkManager& operator=(ChunkManager&&) = delete;

    // Camera coordinates are world-space X/Z. Loading is incremental and
    // performed on the caller's thread; B.6 can move generation off-thread.
    void update(float cameraWorldX, float cameraWorldZ);
    void cleanup() noexcept;

    [[nodiscard]] bool is_chunk_loaded(int chunkX, int chunkZ) const noexcept;
    [[nodiscard]] std::size_t loaded_chunk_count() const noexcept;
    [[nodiscard]] std::uint8_t get_world_block(
        int worldX, int worldY, int worldZ
    ) const noexcept;

    // One merged CPU mesh is uploaded by the renderer as a single GL buffer.
    [[nodiscard]] const std::vector<convergence::Vertex>& mesh_vertices() const;
    [[nodiscard]] std::uint64_t mesh_revision() const noexcept;

    [[nodiscard]] int view_distance() const noexcept;

private:
    struct ChunkColumn {
        std::array<convergence::Chunk, kVerticalSectionCount> sections;
        std::vector<convergence::Vertex> mesh_vertices;
    };

    [[nodiscard]] static std::int64_t floor_div(
        std::int64_t value, std::int64_t divisor
    ) noexcept;

    void unload_distant_chunks(
        int cameraChunkX, int cameraChunkZ,
        std::vector<ChunkCoord>& columns_to_remesh
    );
    void load_chunks_around(
        int cameraChunkX, int cameraChunkZ,
        std::vector<ChunkCoord>& loaded_columns
    );
    void rebuild_meshes_around(
        const std::vector<ChunkCoord>& columns_to_remesh
    );
    void rebuild_column_mesh(int chunkX, int chunkZ);
    [[nodiscard]] std::uint8_t get_world_block64(
        std::int64_t worldX, std::int64_t worldY,
        std::int64_t worldZ
    ) const noexcept;
    void rebuild_combined_mesh() const;

    const WorldGenerator& generator_;
    int viewDistance_;
    std::size_t maxLoadsPerUpdate_;
    std::unordered_map<
        ChunkCoord,
        std::unique_ptr<ChunkColumn>,
        ChunkCoordHash
    > chunks_;

    mutable std::vector<convergence::Vertex> combined_mesh_;
    mutable bool combined_mesh_dirty_ = false;
    std::uint64_t mesh_revision_ = 0;
};
