#include "Camera.h"

#include <algorithm>
#include <cmath>

glm::vec3 Camera::getFrontVector() const {
    const float yawRadians = glm::radians(yaw);
    const float pitchRadians = glm::radians(pitch);

    return glm::normalize(glm::vec3{
        std::cos(pitchRadians) * std::cos(yawRadians),
        std::sin(pitchRadians),
        std::cos(pitchRadians) * std::sin(yawRadians)
    });
}

void Camera::processLook(float deltaX, float deltaY) {
    yaw += deltaX * lookSensitivity;
    pitch -= deltaY * lookSensitivity;
    pitch = std::clamp(pitch, -89.0F, 89.0F);

    // Keep yaw bounded without changing the viewing direction.
    if (yaw > 360.0F || yaw < -360.0F) {
        yaw = std::remainder(yaw, 360.0F);
    }
}

void Camera::processMovement(float forward, float strafe, float deltaTime) {
    if (deltaTime <= 0.0F) {
        return;
    }

    glm::vec3 front = getFrontVector();

    // FPS walking remains horizontal; pitch controls where the player looks,
    // not whether the joystick makes the player fly or sink.
    front.y = 0.0F;
    front = glm::normalize(front);

    const glm::vec3 worldUp{0.0F, 1.0F, 0.0F};
    const glm::vec3 right = glm::normalize(glm::cross(front, worldUp));

    glm::vec3 movement = front * forward + right * strafe;
    const float movementLength = glm::length(movement);
    if (movementLength > 1.0F) {
        movement /= movementLength;
    }

    position += movement * movementSpeed * deltaTime;
}

glm::mat4 Camera::getViewMatrix() const {
    const glm::vec3 worldUp{0.0F, 1.0F, 0.0F};
    return glm::lookAt(position, position + getFrontVector(), worldUp);
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    const float safeAspect = std::max(aspect, 0.001F);
    return glm::perspective(glm::radians(fov), safeAspect, 0.1F, 500.0F);
}
