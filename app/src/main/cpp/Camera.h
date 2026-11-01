#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera final {
public:
    glm::vec3 position{0.0F, 8.0F, 0.0F};
    float yaw = -90.0F;
    float pitch = 0.0F;

    float movementSpeed = 5.0F;
    float lookSensitivity = 0.15F;
    float fov = 70.0F;

    void processLook(float deltaX, float deltaY);
    void processMovement(float forward, float strafe, float deltaTime);

    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspect) const;

private:
    [[nodiscard]] glm::vec3 getFrontVector() const;
};
