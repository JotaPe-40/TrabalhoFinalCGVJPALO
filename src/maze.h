#ifndef MAZE_H
#define MAZE_H

#include <vector>

extern const int mazeW;
extern const int mazeH;
extern const float cellSize;
extern const float wallThickness;
extern const float wallHeight;

extern std::vector<std::vector<int>> wallHorz;
extern std::vector<std::vector<int>> wallVert;

// Marcação paralela a wallHorz/wallVert: 1 quando a parede correspondente
// deve ser desenhada com a textura "globo" em vez da textura padrão de
// tijolos ("brick"). Só tem significado para posições onde a respectiva
// wallHorz[i][j]/wallVert[i][j] é 1 (i.e. onde realmente existe parede).
// Preenchida por AssignWallTextures() logo após cada chamada a GenerateMaze().
extern std::vector<std::vector<int>> wallHorzIsGlobe;
extern std::vector<std::vector<int>> wallVertIsGlobe;

// Fração aproximada (0.0 a 1.0) das paredes do labirinto que devem receber a
// textura "globo" em vez da textura padrão "brick". É apenas uma taxa-alvo:
// o sorteio em AssignWallTextures() respeita a regra de espaçamento mínimo
// abaixo, então a fração final de paredes globo pode ficar menor que esta
// constante caso o labirinto não tenha espaço suficiente para tantas paredes
// globo isoladas. Mantida bem abaixo de 0.5 para garantir que existam
// sempre mais paredes "brick" do que "globo" no labirinto.
extern const float kGlobeWallSpawnRate;

// Número mínimo de paredes "brick" (não-globo) exigido entre quaisquer duas
// paredes "globo", contando ao longo da cadeia de paredes adjacentes (que se
// tocam por um canto comum). Ou seja, duas paredes globo nunca podem ficar
// vizinhas diretas (0 paredes brick entre elas) nem "vizinhas de vizinhas"
// (apenas 1 parede brick entre elas) - é necessário um intervalo de pelo
// menos esta quantidade de paredes brick na cadeia que as separa.
extern const int kMinBrickWallsBetweenGlobeWalls;

// Se verdadeiro, somente as paredes da borda externa do labirinto colidem
// (usado como atalho de depuração, ligado pela tecla ESPAÇO em main.cpp).
extern bool g_OnlyBorderWalls;

// Gera um novo labirinto (algoritmo DFS aleatorizado / randomized
// backtracker), substituindo o conteúdo de wallHorz/wallVert. O parâmetro
// (startRow, startCol) é a célula onde o algoritmo começa a "escavar" os
// corredores; como o algoritmo produz uma árvore de expansão completa (um
// "labirinto perfeito"), isso é apenas um detalhe de implementação - o
// resultado final garante um caminho único entre QUAISQUER duas células do
// grid, independentemente de qual célula tenha sido usada aqui como início.
void GenerateMaze(int startRow = 0, int startCol = 0);

// Sorteia, dentre as paredes existentes (geradas pela última chamada a
// GenerateMaze()), quais devem usar a textura "globo" em vez de "brick",
// preenchendo wallHorzIsGlobe/wallVertIsGlobe. Respeita simultaneamente:
//   (1) a taxa de spawn kGlobeWallSpawnRate (uma fração-alvo do total de
//       paredes do labirinto);
//   (2) o espaçamento mínimo kMinBrickWallsBetweenGlobeWalls entre quaisquer
//       duas paredes globo, medido ao longo da cadeia de paredes adjacentes;
//   (3) a garantia de que o número final de paredes globo é sempre menor que
//       o número de paredes brick.
// Deve ser chamada toda vez que o labirinto é (re)gerado.
void AssignWallTextures();

// Testa se uma caixa alinhada aos eixos (AABB), centrada em (testX, testZ) no
// plano XZ e com semi-larguras "halfX"/"halfZ", colide com alguma parede do
// labirinto. Reaproveitada tanto pela colisão do jogador quanto dos ratos.
bool MazeCollides(float testX, float testZ, float halfX, float halfZ);

#endif