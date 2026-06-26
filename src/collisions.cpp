#include "collisions.h"

#include <cmath>

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
    const std::vector<std::vector<int>> &wallVert)
{
    glm::vec3 pmin =
        glm::vec3(testPos.x - playerHalfWidth,
                  testPos.y,
                  testPos.z - playerHalfDepth);

    glm::vec3 pmax =
        glm::vec3(testPos.x + playerHalfWidth,
                  testPos.y + playerHalfHeight,
                  testPos.z + playerHalfDepth);

    for (int i = 0; i <= mazeH; i++)
    {
        for (int j = 0; j < mazeW; j++)
        {
            if (wallHorz[i][j])
            {
                if (onlyBorderWalls && !(i == 0 || i == mazeH))
                    continue;

                float x = (j - mazeW / 2.0f + 0.5f) * cellSize;
                float z = (i - mazeH / 2.0f) * cellSize;

                float wall_xmin = x - cellSize / 2.0f;
                float wall_xmax = x + cellSize / 2.0f;

                bool overlapX =
                    (pmin.x <= wall_xmax) &&
                    (pmax.x >= wall_xmin);

                float dz = std::fabs(testPos.z - z);

                if (overlapX && dz < playerHalfDepth + eps)
                    return true;
            }
        }
    }

    for (int i = 0; i < mazeH; i++)
    {
        for (int j = 0; j <= mazeW; j++)
        {
            if (wallVert[i][j])
            {
                if (onlyBorderWalls && !(j == 0 || j == mazeW))
                    continue;

                float x = (j - mazeW / 2.0f) * cellSize;
                float z = (i - mazeH / 2.0f + 0.5f) * cellSize;

                float wall_zmin = z - cellSize / 2.0f;
                float wall_zmax = z + cellSize / 2.0f;

                bool overlapZ =
                    (pmin.z <= wall_zmax) &&
                    (pmax.z >= wall_zmin);

                float dx = std::fabs(testPos.x - x);

                if (overlapZ && dx < playerHalfWidth + eps)
                    return true;
            }
        }
    }

    return false;
}

bool CollidesPlayerSphere(
    const glm::vec3 &playerPosition,
    float sphereX,
    float sphereZ,
    float collisionDistance)
{
    float dx = playerPosition.x - sphereX;
    float dz = playerPosition.z - sphereZ;

    float dist = std::sqrt(dx * dx + dz * dz);

    return dist < collisionDistance;
}