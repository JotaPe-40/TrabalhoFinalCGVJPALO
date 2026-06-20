#include "maze.h"

#include <random>
#include <ctime>
#include <cmath>

const int mazeW = 7;
const int mazeH = 7;
const float cellSize = 1.5f;
const float wallThickness = 0.01f;
const float wallHeight = 1.0f;

std::vector<std::vector<int>> wallHorz;
std::vector<std::vector<int>> wallVert;

bool g_OnlyBorderWalls = false;

void GenerateMaze(int startRow, int startCol)
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

    // Célula onde o algoritmo DFS (randomized backtracker) começa a "escavar"
    // o labirinto. Como este algoritmo gera uma árvore de expansão completa
    // (um "labirinto perfeito", sem ciclos), TODAS as células do grid ficam
    // garantidamente conectadas por exatamente um caminho a partir daqui -
    // não apenas a célula inicial, mas o grid completo. Isso significa que,
    // independente de onde o jogador e o "smile" sejam sorteados depois,
    // sempre existirá um caminho entre os dois.
    int sr = startRow;
    int sc = startCol;

    // Defesa extra: garante que a célula inicial está dentro do grid, caso
    // receba parâmetros inválidos (não deveria ocorrer em uso normal).
    if (sr < 0 || sr >= mazeH)
        sr = 0;
    if (sc < 0 || sc >= mazeW)
        sc = 0;

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

bool MazeCollides(float testX, float testZ, float halfX, float halfZ)
{
    const float eps = 0.05f;

    float pminX = testX - halfX;
    float pmaxX = testX + halfX;
    float pminZ = testZ - halfZ;
    float pmaxZ = testZ + halfZ;

    // Paredes horizontais (perpendiculares ao eixo Z, compridas em X)
    for (int i = 0; i <= mazeH; i++)
    {
        for (int j = 0; j < mazeW; j++)
        {
            if (!wallHorz[i][j])
                continue;

            if (g_OnlyBorderWalls && !(i == 0 || i == mazeH))
                continue;

            float x = (j - mazeW / 2.0f + 0.5f) * cellSize;
            float z = (i - mazeH / 2.0f) * cellSize;

            float wall_xmin = x - cellSize / 2.0f;
            float wall_xmax = x + cellSize / 2.0f;

            bool overlapX = (pminX <= wall_xmax) && (pmaxX >= wall_xmin);
            float dz = fabs(testZ - z);

            if (overlapX && dz < (halfZ + eps))
                return true;
        }
    }

    // Paredes verticais (perpendiculares ao eixo X, compridas em Z)
    for (int i = 0; i < mazeH; i++)
    {
        for (int j = 0; j <= mazeW; j++)
        {
            if (!wallVert[i][j])
                continue;

            if (g_OnlyBorderWalls && !(j == 0 || j == mazeW))
                continue;

            float x = (j - mazeW / 2.0f) * cellSize;
            float z = (i - mazeH / 2.0f + 0.5f) * cellSize;

            float wall_zmin = z - cellSize / 2.0f;
            float wall_zmax = z + cellSize / 2.0f;

            bool overlapZ = (pminZ <= wall_zmax) && (pmaxZ >= wall_zmin);
            float dx = fabs(testX - x);

            if (overlapZ && dx < (halfX + eps))
                return true;
        }
    }

    return false;
}