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

void GenerateMaze();

#endif