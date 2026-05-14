#include "maze.h"

#include <random>
#include <ctime>

const int mazeW = 10;
const int mazeH = 10;
const float cellSize = 1.0f;
const float wallThickness = 0.01f;
const float wallHeight = 1.0f;

std::vector<std::vector<int>> wallHorz;
std::vector<std::vector<int>> wallVert;

void GenerateMaze()
{
    std::vector<std::vector<int>> visited(mazeH, std::vector<int>(mazeW, 0));

    wallHorz = std::vector<std::vector<int>>(mazeH + 1, std::vector<int>(mazeW, 1));
    wallVert = std::vector<std::vector<int>>(mazeH, std::vector<int>(mazeW + 1, 1));

    std::vector<std::pair<int, int>> stack;

    auto neighbors = [&](int r, int c)
    {
        std::vector<std::pair<int, int>> nb;

        if (r > 0 && !visited[r - 1][c])
            nb.emplace_back(r - 1, c);
        if (r < mazeH - 1 && !visited[r + 1][c])
            nb.emplace_back(r + 1, c);
        if (c > 0 && !visited[r][c - 1])
            nb.emplace_back(r, c - 1);
        if (c < mazeW - 1 && !visited[r][c + 1])
            nb.emplace_back(r, c + 1);

        return nb;
    };

    std::mt19937 rng((unsigned)time(NULL));

    int sr = 0;
    int sc = 0;

    visited[sr][sc] = 1;
    stack.emplace_back(sr, sc);

    while (!stack.empty())
    {
        auto cell = stack.back();
        int r = cell.first;
        int c = cell.second;
        auto nb = neighbors(r, c);

        if (nb.empty())
        {
            stack.pop_back();
            continue;
        }

        std::uniform_int_distribution<int> dist(0, (int)nb.size() - 1);
        int idx = dist(rng);

        int nr = nb[idx].first;
        int nc = nb[idx].second;

        if (nr == r - 1)
            wallHorz[r][c] = 0;
        else if (nr == r + 1)
            wallHorz[r + 1][c] = 0;
        else if (nc == c - 1)
            wallVert[r][c] = 0;
        else if (nc == c + 1)
            wallVert[r][c + 1] = 0;

        visited[nr][nc] = 1;
        stack.emplace_back(nr, nc);
    }
}