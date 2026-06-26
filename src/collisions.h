#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <vector>
#include <glm/vec3.hpp>

bool CollidesWithMaze(
    const glm::vec3 &testPos,
    float playerHalfWidth,
    float playerHalfHeight,
    float playerHalfDepth,
    float eps,
    int mazeW,
    int mazeH,
    float cellSize,
    bool onlyBorderWalls,
    const std::vector<std::vector<int>> &wallHorz,
    const std::vector<std::vector<int>> &wallVert);

bool CollidesPlayerSphere(
    const glm::vec3 &playerPosition,
    float sphereX,
    float sphereZ,
    float collisionDistance);

#endif