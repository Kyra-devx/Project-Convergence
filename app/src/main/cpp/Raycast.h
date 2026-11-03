#pragma once

#include <glm/glm.hpp>

class ChunkManager;

struct RayHit {
    bool hit = false;
    int x = 0;
    int y = 0;
    int z = 0;
    int nx = 0;
    int ny = 0;
    int nz = 0;
    float distance = 0.0F;
};

// Amanatides-Woo voxel traversal. Direction is normalized internally.
// A hit in the origin cell has a zero normal because no entering face exists.
[[nodiscard]] RayHit raycastVoxels(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const ChunkManager* manager
);
