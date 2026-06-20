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

// Testa se uma caixa alinhada aos eixos (AABB), centrada em (testX, testZ) no
// plano XZ e com semi-larguras "halfX"/"halfZ", colide com alguma parede do
// labirinto. Reaproveitada tanto pela colisão do jogador quanto dos ratos.
bool MazeCollides(float testX, float testZ, float halfX, float halfZ);

#endif