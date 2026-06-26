#include "maze.h"

#include <random>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <queue>
#include <limits>
#include <cstdint>

const int mazeW = 7;
const int mazeH = 7;
const float cellSize = 1.5f;
const float wallThickness = 0.01f;
const float wallHeight = 1.0f;

std::vector<std::vector<int>> wallHorz;
std::vector<std::vector<int>> wallVert;

std::vector<std::vector<int>> wallHorzIsGlobe;
std::vector<std::vector<int>> wallVertIsGlobe;

// Taxa-alvo de spawn de paredes "globo": ~18% das paredes do labirinto.
// Mantida bem abaixo de 0.5 para que o labirinto sempre tenha mais paredes
// "brick" do que "globo" (requisito explícito), mesmo antes de aplicar a
// regra de espaçamento mínimo abaixo (que só reduz esse número, nunca
// aumenta).
const float kGlobeWallSpawnRate = 0.18f;

// Exige pelo menos 2 paredes "brick" entre quaisquer duas paredes "globo" ao
// longo da cadeia de paredes adjacentes (paredes que se tocam por um canto
// comum do grid).
const int kMinBrickWallsBetweenGlobeWalls = 2;

bool g_OnlyBorderWalls = false;

void GenerateMaze(int startRow, int startCol)
{
    std::vector<std::vector<int>> visited(mazeH, std::vector<int>(mazeW, 0));

    wallHorz = std::vector<std::vector<int>>(mazeH + 1, std::vector<int>(mazeW, 1));
    wallVert = std::vector<std::vector<int>>(mazeH, std::vector<int>(mazeW + 1, 1));

    // As marcações de textura "globo" são recalculadas do zero a cada novo
    // labirinto (ver AssignWallTextures(), chamada separadamente por quem
    // gera o labirinto): aqui só garantimos que as matrizes já existem com o
    // tamanho certo e sem nenhuma parede globo, para o caso de o labirinto
    // ser desenhado antes da primeira chamada a AssignWallTextures().
    wallHorzIsGlobe = std::vector<std::vector<int>>(mazeH + 1, std::vector<int>(mazeW, 0));
    wallVertIsGlobe = std::vector<std::vector<int>>(mazeH, std::vector<int>(mazeW + 1, 0));

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

// ---------------------------------------------------------------------------
// Texturas de parede (brick x globo)
// ---------------------------------------------------------------------------
//
// Cada parede existente é identificada por um "ID de parede" simples: as
// paredes horizontais vêm primeiro (varrendo wallHorz linha a linha), depois
// as verticais (varrendo wallVert linha a linha). Cada parede também sabe os
// dois cantos do grid que ela ocupa (em coordenadas inteiras de canto, de
// (0,0) a (mazeH,mazeW)); duas paredes são consideradas "adjacentes" quando
// compartilham um desses cantos - é essa noção de adjacência que usamos para
// medir "quantas paredes existem entre" duas paredes globo, via BFS.
namespace
{
    struct WallRef
    {
        bool isHorz; // true = wallHorz[i][j], false = wallVert[i][j]
        int i, j;
        int cornerR0, cornerC0; // primeiro canto ocupado pela parede
        int cornerR1, cornerC1; // segundo canto ocupado pela parede
    };
} // namespace

void AssignWallTextures()
{
    wallHorzIsGlobe = std::vector<std::vector<int>>(mazeH + 1, std::vector<int>(mazeW, 0));
    wallVertIsGlobe = std::vector<std::vector<int>>(mazeH, std::vector<int>(mazeW + 1, 0));

    // 1) Coleta todas as paredes que de fato existem no labirinto atual.
    std::vector<WallRef> walls;

    for (int i = 0; i <= mazeH; i++)
        for (int j = 0; j < mazeW; j++)
            if (wallHorz[i][j])
                walls.push_back(WallRef{true, i, j, i, j, i, j + 1});

    for (int i = 0; i < mazeH; i++)
        for (int j = 0; j <= mazeW; j++)
            if (wallVert[i][j])
                walls.push_back(WallRef{false, i, j, i, j, i + 1, j});

    int totalWalls = (int)walls.size();
    if (totalWalls == 0)
        return;

    // 2) Constrói a lista de adjacência entre paredes (compartilham um
    // canto). Como o número de paredes é pequeno (labirinto W x H), um
    // algoritmo O(N^2) é suficiente e mantém o código simples.
    std::vector<std::vector<int>> adjacency(totalWalls);
    for (int a = 0; a < totalWalls; a++)
    {
        for (int b = a + 1; b < totalWalls; b++)
        {
            const WallRef &wa = walls[a];
            const WallRef &wb = walls[b];

            bool shareCorner =
                (wa.cornerR0 == wb.cornerR0 && wa.cornerC0 == wb.cornerC0) ||
                (wa.cornerR0 == wb.cornerR1 && wa.cornerC0 == wb.cornerC1) ||
                (wa.cornerR1 == wb.cornerR0 && wa.cornerC1 == wb.cornerC0) ||
                (wa.cornerR1 == wb.cornerR1 && wa.cornerC1 == wb.cornerC1);

            if (shareCorner)
            {
                adjacency[a].push_back(b);
                adjacency[b].push_back(a);
            }
        }
    }

    // 3) Distância mínima (em número de "saltos" parede-a-parede) de cada
    // parede até a parede globo mais próxima já escolhida. Usada para
    // impedir que duas paredes globo fiquem mais próximas do que o
    // espaçamento mínimo exigido. Começa em "infinito" (nenhuma parede globo
    // escolhida ainda).
    const int kInfinity = std::numeric_limits<int>::max();
    std::vector<int> distToNearestGlobe(totalWalls, kInfinity);

    // Uma parede só pode se tornar globo se sua distância até a parede globo
    // mais próxima for maior que kMinBrickWallsBetweenGlobeWalls: por
    // exemplo, com o mínimo padrão de 2 paredes brick entre paredes globo,
    // só aceitamos uma nova parede globo a uma distância >= 3 saltos de
    // qualquer parede globo já escolhida (2 paredes brick no meio do
    // caminho + 1 salto final até a nova parede globo).
    const int kMinHopDistance = kMinBrickWallsBetweenGlobeWalls + 1;

    // 4) Número máximo de paredes globo permitido pela taxa de spawn
    // configurada, sempre arredondado para baixo e nunca permitindo que
    // paredes globo sejam maioria (garante mais brick do que globo mesmo em
    // labirintos muito pequenos).
    int maxGlobeWallsByRate = (int)std::floor(totalWalls * kGlobeWallSpawnRate);
    int maxGlobeWallsByMajority = (totalWalls - 1) / 2; // estritamente menos da metade
    int maxGlobeWalls = std::min(maxGlobeWallsByRate, maxGlobeWallsByMajority);
    maxGlobeWalls = std::max(maxGlobeWalls, 0);

    if (maxGlobeWalls == 0)
        return;

    // 5) Sorteia a ORDEM em que as paredes são consideradas como candidatas
    // a globo (em vez de sempre testar na mesma ordem geométrica), para que
    // o conjunto final de paredes globo varie entre labirintos/partidas.
    std::vector<int> order(totalWalls);
    for (int k = 0; k < totalWalls; k++)
        order[k] = k;

    std::mt19937 rng((unsigned)time(NULL) ^ (unsigned)(uintptr_t)&walls);
    std::shuffle(order.begin(), order.end(), rng);

    int globeCount = 0;

    for (int idx : order)
    {
        if (globeCount >= maxGlobeWalls)
            break;

        if (distToNearestGlobe[idx] <= kMinHopDistance)
            continue; // muito perto (em saltos) de uma parede globo já escolhida

        // Aceita esta parede como globo.
        const WallRef &w = walls[idx];
        if (w.isHorz)
            wallHorzIsGlobe[w.i][w.j] = 1;
        else
            wallVertIsGlobe[w.i][w.j] = 1;

        globeCount++;

        // Recalcula, via BFS a partir desta nova parede globo, a distância
        // mínima de todas as paredes até a parede globo mais próxima -
        // atualizando apenas onde a nova distância é menor que a anterior.
        std::vector<int> distFromHere(totalWalls, kInfinity);
        distFromHere[idx] = 0;
        std::queue<int> bfsQueue;
        bfsQueue.push(idx);

        while (!bfsQueue.empty())
        {
            int cur = bfsQueue.front();
            bfsQueue.pop();

            // Não precisa expandir além do necessário: distâncias maiores
            // que kMinHopDistance não vão mais ser usadas para bloquear
            // nada (qualquer parede já distante o suficiente).
            if (distFromHere[cur] >= kMinHopDistance)
                continue;

            for (int neighborIdx : adjacency[cur])
            {
                if (distFromHere[neighborIdx] == kInfinity)
                {
                    distFromHere[neighborIdx] = distFromHere[cur] + 1;
                    bfsQueue.push(neighborIdx);
                }
            }
        }

        for (int k = 0; k < totalWalls; k++)
            distToNearestGlobe[k] = std::min(distToNearestGlobe[k], distFromHere[k]);
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