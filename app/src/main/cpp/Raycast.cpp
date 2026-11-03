#include "Raycast.h"

#include "ChunkManager.h"
#include "VoxelDims.h"

#include <cmath>
#include <limits>

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kDirectionEpsilon = 1.0e-12;

bool to_cell(float coordinate, int& cell) noexcept {
    if (!std::isfinite(coordinate)) return false;
    const double floored = std::floor(static_cast<double>(coordinate));
    if (floored < static_cast<double>(std::numeric_limits<int>::min()) ||
        floored > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    cell = static_cast<int>(floored);
    return true;
}

}  // namespace

RayHit raycastVoxels(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const ChunkManager* manager
) {
    RayHit result;
    if (manager == nullptr || !std::isfinite(maxDistance) ||
        maxDistance < 0.0F ||
        !std::isfinite(origin.x) || !std::isfinite(origin.y) ||
        !std::isfinite(origin.z) ||
        !std::isfinite(direction.x) || !std::isfinite(direction.y) ||
        !std::isfinite(direction.z)) {
        return result;
    }

    const double dxInput = static_cast<double>(direction.x);
    const double dyInput = static_cast<double>(direction.y);
    const double dzInput = static_cast<double>(direction.z);
    const double directionLength = std::sqrt(
        dxInput * dxInput + dyInput * dyInput + dzInput * dzInput);
    if (!std::isfinite(directionLength) ||
        directionLength <= kDirectionEpsilon) {
        return result;
    }

    // Normalize so maxDistance is measured in world units for every caller.
    const double dx = dxInput / directionLength;
    const double dy = dyInput / directionLength;
    const double dz = dzInput / directionLength;

    int x = 0;
    int y = 0;
    int z = 0;
    if (!to_cell(origin.x, x) ||
        !to_cell(origin.y, y) ||
        !to_cell(origin.z, z)) {
        return result;
    }

    const int stepX = dx > 0.0 ? 1 : (dx < 0.0 ? -1 : 0);
    const int stepY = dy > 0.0 ? 1 : (dy < 0.0 ? -1 : 0);
    const int stepZ = dz > 0.0 ? 1 : (dz < 0.0 ? -1 : 0);

    const double tDeltaX = stepX == 0 ? kInfinity : std::abs(1.0 / dx);
    const double tDeltaY = stepY == 0 ? kInfinity : std::abs(1.0 / dy);
    const double tDeltaZ = stepZ == 0 ? kInfinity : std::abs(1.0 / dz);

    double nextX = stepX > 0
        ? (static_cast<double>(x) + 1.0 - origin.x) / dx
        : (stepX < 0 ? (static_cast<double>(x) - origin.x) / dx : kInfinity);
    double nextY = stepY > 0
        ? (static_cast<double>(y) + 1.0 - origin.y) / dy
        : (stepY < 0 ? (static_cast<double>(y) - origin.y) / dy : kInfinity);
    double nextZ = stepZ > 0
        ? (static_cast<double>(z) + 1.0 - origin.z) / dz
        : (stepZ < 0 ? (static_cast<double>(z) - origin.z) / dz : kInfinity);

    const double maximumDistance = static_cast<double>(maxDistance);
    double travelled = 0.0;
    int faceNX = 0;
    int faceNY = 0;
    int faceNZ = 0;

    for (;;) {
        if (y >= 0 && y < cvg::SY &&
            manager->get_world_block(x, y, z) != 0U) {
            result.hit = true;
            result.x = x;
            result.y = y;
            result.z = z;
            result.nx = faceNX;
            result.ny = faceNY;
            result.nz = faceNZ;
            result.distance = static_cast<float>(travelled);
            // An origin-cell hit has no entry face; placement is rejected.
            return result;
        }

        // Deterministic X, then Y, then Z tie-breaking for edge/corner rays.
        if (nextX <= nextY && nextX <= nextZ) {
            if (nextX > maximumDistance ||
                (stepX > 0 && x == std::numeric_limits<int>::max()) ||
                (stepX < 0 && x == std::numeric_limits<int>::min())) {
                break;
            }
            travelled = nextX;
            x += stepX;
            nextX += tDeltaX;
            faceNX = -stepX;
            faceNY = 0;
            faceNZ = 0;
        } else if (nextY <= nextZ) {
            if (nextY > maximumDistance ||
                (stepY > 0 && y == std::numeric_limits<int>::max()) ||
                (stepY < 0 && y == std::numeric_limits<int>::min())) {
                break;
            }
            travelled = nextY;
            y += stepY;
            nextY += tDeltaY;
            faceNX = 0;
            faceNY = -stepY;
            faceNZ = 0;
        } else {
            if (nextZ > maximumDistance ||
                (stepZ > 0 && z == std::numeric_limits<int>::max()) ||
                (stepZ < 0 && z == std::numeric_limits<int>::min())) {
                break;
            }
            travelled = nextZ;
            z += stepZ;
            nextZ += tDeltaZ;
            faceNX = 0;
            faceNY = 0;
            faceNZ = -stepZ;
        }
    }

    return result;
}
