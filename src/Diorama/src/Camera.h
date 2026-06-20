#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    Camera(glm::vec3 startPos = glm::vec3(0.0f, 2.0f, 10.0f))
        : position(startPos),
          front(glm::vec3(0.0f, 0.0f, -1.0f)),
          up(glm::vec3(0.0f, 1.0f, 0.0f)),
          yaw(-90.0f), pitch(-8.0f),
          speed(6.0f), sensitivity(0.08f)
    {
        updateVectors();
    }

    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    void move(int direction, float deltaTime)
    {
        float velocity = speed * deltaTime;
        switch (direction)
        {
            case 0: position += front * velocity; break;
            case 1: position -= front * velocity; break;
            case 2: position -= right * velocity; break;
            case 3: position += right * velocity; break;
        }
    }

    void rotate(float xOffset, float yOffset)
    {
        yaw   += xOffset * sensitivity;
        pitch += yOffset * sensitivity;
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }

private:
    void updateVectors()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        up    = glm::normalize(glm::cross(right, front));
    }
};
