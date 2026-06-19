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

void GenerateMaze();

// Testa se uma caixa alinhada aos eixos (AABB), centrada em (testX, testZ) no
// plano XZ e com semi-larguras "halfX"/"halfZ", colide com alguma parede do
// labirinto. Reaproveitada tanto pela colisão do jogador quanto dos ratos.
bool MazeCollides(float testX, float testZ, float halfX, float halfZ);

#endif