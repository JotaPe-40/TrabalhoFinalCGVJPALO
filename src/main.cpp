//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Computação Gráfica e Visualização I
//               Prof. Eduardo Gastal
//
//     CÓDIGO BASE PARA O TRABALHO FINAL
//

// Arquivos "headers" padrões de C podem ser incluídos em um
// programa C++, sendo necessário somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Headers abaixo são específicos de C++
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <random>
#include <ctime>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>  // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h> // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .

// NOSSOS INCLUDES
#include "maze.h"
#include "audio.h"

struct ObjModel
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char *filename, const char *basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i + 1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                        filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("OK.\n");
    }
};

// Estrutura com os dados de um ratinho que se movimenta aleatoriamente pelo
// labirinto, seguindo uma curva de Bézier cúbica entre pontos sorteados.
// Definida aqui (antes dos protótipos abaixo) pois é usada como parâmetro
// em algumas das funções declaradas a seguir.
struct Rat
{
    // Pontos de controle da curva de Bézier cúbica que define o trecho de
    // movimento atual (em coordenadas de mundo, no plano XZ; Y é fixo no chão).
    glm::vec3 p0, p1, p2, p3;
    float t;             // parâmetro [0,1] dentro do trecho atual da curva
    float duration;      // duração (segundos) do trecho atual (independente de FPS)
    glm::vec3 position;  // posição atual (cache, atualizada em UpdateRats)
    float yaw;           // orientação para desenhar o modelo virado para onde anda

    // Estado de "assustado": ativado quando o jogador colide com o rato
    // (teste de intersecção cubo-cubo). Enquanto assustado, o rato foge na
    // direção oposta ao jogador, com velocidade maior, ignorando o sorteio
    // normal de destino aleatório até o temporizador abaixo zerar.
    bool scared = false;
    float scaredTimer = 0.0f; // segundos restantes em estado de fuga
};

// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4 &M);
void BuildCubeAndAddToVirtualScene(float size, const char *name);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel *); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel *model);                // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles();                         // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char *filename);         // Função que carrega imagens de textura
void DrawVirtualObject(const char *object_name);     // Desenha um objeto armazenado em g_VirtualScene
glm::vec3 ConstrainPlayerToGround(glm::vec3 candidate_position);
GLuint LoadShader_Vertex(const char *filename);                              // Carrega um vertex shader
GLuint LoadShader_Fragment(const char *filename);                            // Carrega um fragment shader
void LoadShader(const char *filename, GLuint shader_id);                     // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel *);                                          // Função para debugging

// NOSSAS FUNÇÕES: ratos, colisão jogador-smile e tela de fim de jogo.
glm::vec3 ComputeSmilePosition();                                   // Posição (mundo) do centro do smile na célula configurada
glm::vec3 ComputeLanternPosition();                                  // Posição (mundo) da lanterna do personagem, fonte de luz principal da cena
glm::vec3 ComputeLanternDirection();                                 // Direção (mundo, normalizada) para onde a lanterna aponta - segue o olhar do jogador (yaw + pitch), para o efeito de spotlight no fragment shader
float ComputeLightVisibility(glm::vec3 lightPos, glm::vec3 targetPos, float sampleRadius); // Fração (0-1) de quanto a luz alcança um objeto, suavizando a transição com a sombra
void UpdateVertexLightWeights(const char *object_name, const glm::mat4 &model, glm::vec3 lightPos); // Recalcula e envia à GPU o peso de luz POR VÉRTICE de um objeto (cubo), para sombras suaves ao longo da própria face
void RandomizeStartAndGoalCells();                                   // Sorteia células (distintas) para o jogador e o smile, e regenera o labirinto a partir da célula do jogador
void SpawnRats();                                                   // Cria/recria os ratinhos em posições aleatórias válidas
glm::vec3 RandomPointInsideMaze(std::mt19937 &rng);                 // Sorteia um ponto de mundo dentro do labirinto
void PickNewBezierLeg(Rat &rat, std::mt19937 &rng);                 // Sorteia um novo trecho de curva de Bézier para um rato
void PickFleeBezierLeg(Rat &rat, std::mt19937 &rng, glm::vec3 awayFrom); // Sorteia um trecho de fuga, afastando o rato do ponto "awayFrom"
void UpdateRats(float dt);                                          // Avança a simulação de todos os ratos em dt segundos
void CheckPlayerRatCollisions();                                    // Teste cubo-cubo jogador-rato: ao colidir, assusta o rato (foge)
bool IsRatVisibleToPlayer(const Rat &rat, glm::vec3 playerEyePos, glm::vec3 viewDir); // Rato dentro do campo de visão e sem parede bloqueando
bool SphereAabbIntersect(glm::vec3 sphereCenter, float sphereRadius,
                          glm::vec3 boxCenter, glm::vec3 halfExtents); // Teste de intersecção cubo-esfera
bool AabbAabbIntersect(glm::vec3 centerA, glm::vec3 halfA,
                        glm::vec3 centerB, glm::vec3 halfB);           // Teste de intersecção cubo-cubo
void ResetGame(GLFWwindow *window);                                  // Reinicia labirinto, jogador, ratos e cronômetro
void StartGameFromMenu();                                            // Inicia a primeira partida a partir da tela de menu (sorteia labirinto, ratos, cronômetro e música)
void DrawGameOverScreen(GLFWwindow *window, double elapsedSeconds); // Desenha overlay de fim de jogo + botão "jogar novamente"
void DrawMenuScreen(GLFWwindow *window); // Desenha a tela de menu inicial: objetivo, controles, botão "Iniciar" e caixa de seleção de música
void DrawColoredQuad2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a); // Desenha um quad 2D colorido (para o botão)
void DrawPlayerCharacter(glm::vec3 position, float yaw, const glm::mat4 &view, const glm::mat4 &projection); // Desenha o boneco explorador do jogador

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow *window);
float TextRendering_CharWidth(GLFWwindow *window);
void TextRendering_PrintString(GLFWwindow *window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow *window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow *window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow *window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow *window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow *window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow *window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow *window);
void TextRendering_ShowProjection(GLFWwindow *window);
void TextRendering_ShowFramesPerSecond(GLFWwindow *window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow *window, int width, int height);
void ErrorCallback(int error, const char *description);
void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos);
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string name;              // Nome do objeto
    size_t first_index;            // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t num_indices;            // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum rendering_mode;         // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3 bbox_min;            // Axis-Aligned Bounding Box do objeto
    glm::vec3 bbox_max;
    // VBO dinâmico (GL_DYNAMIC_DRAW) com o peso de visibilidade de luz POR
    // VÉRTICE (location 3 no vertex shader), usado para suavizar a sombra
    // ao longo da própria superfície de um objeto (em vez de um único valor
    // uniforme por chamada de desenho). 0 quando o objeto não usa esse
    // atributo (mantém o valor antigo do uniform "light_visibility" via
    // fallback no fragment shader). Ver UpdateVertexLightWeights().
    GLuint vertex_light_weight_vbo_id = 0;
    size_t num_vertices = 0; // número de vértices (não de índices) - tamanho esperado do VBO acima
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Posições locais (espaço do modelo) de cada vértice de "wall_cube" (e de
// qualquer outro objeto construído por BuildCubeAndAddToVirtualScene), na
// MESMA ordem em que aparecem no VBO de posições - usadas por
// UpdateVertexLightWeights() para calcular o peso de luz de cada vértice a
// partir da sua posição local dentro do cubo (qual extremidade da parede ele
// ocupa), sem precisar reconstruir a geometria a cada frame.
std::map<std::string, std::vector<glm::vec3>> g_CubeLocalVertexPositions;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4> g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false;  // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Variáveis que definem a câmera em coordenadas esféricas, controladas pelo
// usuário através do mouse (veja função CursorPosCallback()). A posição
// efetiva da câmera é calculada dentro da função main(), dentro do loop de
// renderização.
float g_CameraTheta = 0.0f;    // Ângulo no plano ZX em relação ao eixo Z
float g_CameraPhi = 0.0f;      // Ângulo em relação ao eixo Y
float g_CameraDistance = 3.5f; // Distância da câmera para a origem

// First-person player / camera
float g_GroundY = -1.1f;
glm::vec3 g_PlayerPosition = glm::vec3(0.0f, g_GroundY, 0.0f);
float g_PlayerYaw = 3.1415926f; // Facing -Z by default
float g_PlayerPitch = 0.0f;
float g_PlayerSpeed = 3.0f; // units per second
float g_PlayerEyeHeight = 0.6f;
// Hitbox do jogador: um CUBO (mesma semi-extensão nos três eixos X, Y e Z).
// O valor de "g_PlayerHalfHeight" é usado em dobro como a altura total do
// cubo (do chão até o topo da cabeça), e as semi-larguras em X/Z usam o
// mesmo valor para fechar o cubo.
float g_PlayerHalfWidth = 0.30f;
float g_PlayerHalfHeight = 0.30f;
float g_PlayerHalfDepth = 0.30f;

// ===========================================================================
// Animação de andar do boneco (ver DrawPlayerCharacter()): em vez de uma
// pose estática, braços e pernas oscilam como um pêndulo (cada perna em
// fase oposta ao braço do mesmo lado, como em uma caminhada normal) sempre
// que o jogador está efetivamente se movendo pelo labirinto.
//
// "g_PlayerWalkPhase" acumula ao longo do tempo (em radianos) e é a entrada
// de uma função seno usada para gerar o balanço de cada membro; avança com
// base na DISTÂNCIA percorrida pelo jogador (não diretamente no tempo), de
// modo que o ciclo da passada acompanhe a velocidade real do personagem
// (caminhando rápido = passadas mais rápidas) e fique parado quando o
// jogador está parado, em vez de depender do framerate.
float g_PlayerWalkPhase = 0.0f;

// Quão rápido a fase de caminhada avança por unidade de distância percorrida
// (radianos por unidade de mundo). Calibrado para parecer uma passada normal
// na velocidade padrão do jogador (g_PlayerSpeed).
const float g_PlayerWalkPhaseSpeed = 9.0f;

// Amplitude máxima (radianos) do balanço de pernas/braços ao redor do quadril/
// ombro durante a animação de andar.
const float g_PlayerWalkSwingAmplitude = 0.55f;
bool g_FpsMode = true; // enable FPS camera and controls
bool g_FirstMouse = true;
// Last cursor positions (used by mouse callbacks and FPS init)
double g_LastCursorPosX = 0.0;
double g_LastCursorPosY = 0.0;

// Variáveis que controlam rotação do antebraço
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// ===========================================================================
// Smile (objetivo do jogador). Hitbox: ESFERA.
// ===========================================================================
int g_SphereRow = 2;
int g_SphereCol = 2;
glm::vec3 g_SmileCenter = glm::vec3(0.0f, 0.0f, 0.0f); // recomputado a cada frame
float g_SmileRadius = 0.0f;                            // recomputado a cada frame, em unidades de mundo

// ===========================================================================
// Ratinhos: vários, espalhados pelo labirinto, movendo-se aleatoriamente.
// Hitbox: QUADRADO (AABB com mesma semi-largura em X e Z), colidindo
// apenas com as paredes do labirinto (por enquanto).
// ===========================================================================
std::vector<Rat> g_Rats;
const float g_RatHalfSize = 0.18f;   // hitbox quadrada (mesma semi-largura em X e Z)
const float g_RatScale = 0.0046f;    // escala do modelo 3D (rat.obj) para ~0.45 unidades de comprimento
const int g_NumRats = 6;
const float g_RatScaredDuration = 3.5f; // segundos que o rato passa fugindo após ser tocado pelo jogador

// ===========================================================================
// Estado de jogo / cronômetro / tela de fim de jogo
// ===========================================================================
enum GameState
{
    GAME_MENU = 2, // Tela inicial (objetivo, controles, botão "Iniciar" e checkbox de música), antes da partida começar
    GAME_PLAYING = 0,
    GAME_OVER = 1
};
GameState g_GameState = GAME_MENU;
double g_GameStartTime = 0.0;   // glfwGetTime() no início da partida atual
double g_GameOverElapsed = 0.0; // tempo total decorrido, congelado no momento do game over

// Área (em coordenadas de tela, NDC: x em [-1,1], y em [-1,1]) do botão
// "Jogar novamente" mostrado na tela de fim de jogo. Calculada em
// DrawGameOverScreen() e usada em MouseButtonCallback() para detectar clique.
float g_RestartButtonMinX = 0.0f, g_RestartButtonMaxX = 0.0f;
float g_RestartButtonMinY = 0.0f, g_RestartButtonMaxY = 0.0f;
bool g_RestartButtonValid = false;

// ===========================================================================
// Tela de menu inicial (ver DrawMenuScreen())
// ===========================================================================
// Liga/desliga a música de fundo do jogo. Controlada pela caixa de seleção
// na tela de menu; também pode ser alternada a qualquer momento (mesmo
// depois de iniciar a partida) clicando na mesma caixa, caso o menu seja
// reaberto - mas por padrão a tela de menu só aparece uma vez, antes da
// primeira partida.
bool g_MusicEnabled = true;

// Áreas clicáveis da tela de menu (botão "Iniciar" e a caixa de seleção de
// música), calculadas em DrawMenuScreen() e usadas em MouseButtonCallback().
float g_StartButtonMinX = 0.0f, g_StartButtonMaxX = 0.0f;
float g_StartButtonMinY = 0.0f, g_StartButtonMaxY = 0.0f;
bool g_StartButtonValid = false;

float g_MusicCheckboxMinX = 0.0f, g_MusicCheckboxMaxX = 0.0f;
float g_MusicCheckboxMinY = 0.0f, g_MusicCheckboxMaxY = 0.0f;
bool g_MusicCheckboxValid = false;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLint g_texture_repeat_uniform;
GLint g_light_position_uniform;  // posição da lanterna do personagem, em coordenadas de mundo
GLint g_light_direction_uniform; // direção (normalizada) para onde a lanterna aponta, em coordenadas de mundo - permite tratá-la como um SPOTLIGHT (cone de luz) em vez de uma luz pontual onidirecional
GLint g_light_visibility_uniform; // fração (0.0-1.0) de quanto a luz da lanterna alcança o objeto desenhado (suaviza a transição com a sombra das paredes)

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

bool g_TopView = false;


std::string FindFile(const std::string &path)
{
    std::vector<std::string> prefixes = {
        "",
        "../",
        "../../"};

    for (const std::string &prefix : prefixes)
    {
        std::string fullpath = prefix + path;
        std::ifstream file(fullpath);
        if (file.good())
            return fullpath;
    }

    fprintf(stderr, "ERROR: Cannot find file \"%s\".\n", path.c_str());
    std::exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    // Inicializa o sistema de áudio (trilha de fundo, efeitos sonoros dos
    // ratos, som de vitória). Se falhar (ex.: sem dispositivo de áudio
    // disponível), o jogo continua normalmente, apenas sem som.
    Audio_Init();

    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 800 colunas e 600 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow *window;
    window = glfwCreateWindow(800, 600, "INF01047 - Seu Cartao - Seu Nome", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor = glGetString(GL_VENDOR);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *glversion = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    //
    LoadShadersFromFiles();

    // Define o valor GENÉRICO padrão (1.0 = totalmente iluminado / "sem
    // efeito") do atributo de vértice "vertex_light_weight" (location 3 no
    // vertex shader). Este valor só é realmente usado pela GPU quando o
    // array de atributos correspondente está DESABILITADO no VAO ativo (ou
    // seja, para todo objeto que não usa o VBO dinâmico de peso de luz por
    // vértice criado em BuildCubeAndAddToVirtualScene/UpdateVertexLightWeights,
    // como a esfera do smile, o rato, o teto, etc.) - garantindo que esses
    // objetos continuem se comportando exatamente como antes (dependendo
    // apenas do uniform "light_visibility" por objeto).
    glVertexAttrib1f(3, 1.0f);

    LoadTextureImage(FindFile("data/parede.png").c_str());   // TextureImage0
    LoadTextureImage(FindFile("assets/sand.png").c_str());   // TextureImage1
    LoadTextureImage(FindFile("assets/teto.png").c_str());   // TextureImage2
    LoadTextureImage(FindFile("assets/smile.png").c_str());  // TextureImage3
    LoadTextureImage(FindFile("assets/fur.png").c_str());     // TextureImage4
    LoadTextureImage(FindFile("assets/globo.png").c_str());   // TextureImage5 (paredes "globo", sorteadas aleatoriamente em AssignWallTextures())

    // Construímos apenas o chão/grama.
    std::string plane_path = FindFile("data/plane.obj");
    ObjModel planemodel(plane_path.c_str());

    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    // Usa a esfera do trabalho base
    std::string sphere_path = FindFile("data/sphere.obj");
    ObjModel spheremodel(sphere_path.c_str());

    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    // Constrói o cubo usado para as paredes do labirinto
    BuildCubeAndAddToVirtualScene(1.0f, "wall_cube");

    // Reaproveita a MESMA geometria/infra do cubo (incluindo o VBO dinâmico
    // de peso de luz por vértice) para um "ladrilho" de chão: o chão deixa
    // de ser desenhado como um único quad gigante cobrindo o labirinto
    // inteiro (data/plane.obj, escalado) e passa a ser desenhado CÉLULA A
    // CÉLULA, um pequeno cubo bem fino (achatado em Y) por célula do
    // labirinto. Isso permite calcular e vetorizar a visibilidade da luz
    // por vértice TAMBÉM no chão, em vez de um valor fixo "totalmente
    // iluminado" - corrigindo o vazamento de luz por baixo das paredes
    // (antes a luz da lanterna atravessava paredes no plano do piso, já
    // que o chão nunca testava oclusão).
    BuildCubeAndAddToVirtualScene(1.0f, "floor_tile");

    // Carrega o modelo 3D do ratinho (convertido de assets/rat.stl para
    // data/rat.obj, com eixos já remapeados para a convenção do motor:
    // Y para cima e a cabeça apontando para -Z).
    std::string rat_path = FindFile("data/rat.obj");
    ObjModel ratmodel(rat_path.c_str());

    ComputeNormals(&ratmodel);
    BuildTrianglesAndAddToVirtualScene(&ratmodel);

    if (argc > 1)
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // O cursor do mouse só é capturado/escondido (modo FPS) quando a
    // partida realmente começa - enquanto a tela de menu inicial estiver
    // visível (g_GameState == GAME_MENU), o cursor precisa ficar livre e
    // visível para que o jogador possa clicar no botão "Iniciar" e na
    // caixa de seleção de música. A captura do cursor acontece em
    // MouseButtonCallback() no momento exato em que StartGameFromMenu() é
    // chamada.
    if (g_FpsMode && g_GameState != GAME_MENU)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_FirstMouse = true;
    }

    double lastTime = glfwGetTime();

    // O jogo começa na tela de menu (g_GameState já inicializado como
    // GAME_MENU). Geramos um labirinto e posições válidas desde já apenas
    // para que a cena 3D já apareça (desfocada por baixo do painel do
    // menu) atrás da tela inicial, em vez de uma tela vazia - mas o
    // cronômetro, os ratos e a música de fundo só começam quando o
    // jogador realmente clica em "Iniciar" (ver StartGameFromMenu(),
    // chamada em MouseButtonCallback()).
    RandomizeStartAndGoalCells();
    SpawnRats();

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(0.9f, 0.9f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        GLint boolLocation = glGetUniformLocation(g_GpuProgramID, "c_top");
        glUniform1i(boolLocation, g_TopView ? 1 : 0);

        // Compute delta time
        double currentTime = glfwGetTime();
        float dt = (float)(currentTime - lastTime);
        lastTime = currentTime;

        // FPS camera: compute front vector from yaw/pitch
        glm::vec3 front3;
        front3.x = cosf(g_PlayerPitch) * sinf(g_PlayerYaw);
        front3.y = sinf(g_PlayerPitch);
        front3.z = cosf(g_PlayerPitch) * cosf(g_PlayerYaw);
        front3 = glm::normalize(front3);

        // Movement: WASD
        glm::vec4 front4 = glm::vec4(front3, 0.0f);
        glm::vec4 up4 = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        glm::vec4 right4 = crossproduct(front4, up4);
        float rlen = norm(right4);
        glm::vec3 right3 = (rlen > 1e-6f) ? glm::vec3(right4.x / rlen, right4.y / rlen, right4.z / rlen) : glm::vec3(1.0f, 0.0f, 0.0f);

        glm::vec3 forward_xz = glm::normalize(glm::vec3(front3.x, 0.0f, front3.z));

        float speed = g_PlayerSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed *= 2.0f;

        glm::vec3 prevPos = g_PlayerPosition;
        glm::vec3 playerPosBeforeMove = g_PlayerPosition; // usado só para medir a distância percorrida neste frame (animação de andar)
        glm::vec3 moveDelta = glm::vec3(0.0f, 0.0f, 0.0f);

        // Quando o jogo termina (jogador encostou no smile), o movimento do
        // jogador é congelado: nenhuma tecla de movimento tem efeito. A
        // visão de cima (TAB) troca apenas a câmera, por uma câmera de
        // espectador fixa olhando para baixo; o jogador continua podendo se
        // mover normalmente com WASD enquanto essa visão estiver ativa (o
        // movimento é relativo à direção que o personagem estava olhando
        // antes de entrar na visão de cima, já que essa câmera não tem
        // yaw/pitch próprios controláveis pelo mouse).
        bool inputEnabled = (g_GameState == GAME_PLAYING);

        if (inputEnabled)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                moveDelta += forward_xz * speed * dt;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                moveDelta -= forward_xz * speed * dt;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                moveDelta -= glm::normalize(glm::vec3(right3.x, 0.0f, right3.z)) * speed * dt;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                moveDelta += glm::normalize(glm::vec3(right3.x, 0.0f, right3.z)) * speed * dt;
        }

        // Testa colisão do jogador (hitbox cúbica) contra as paredes do
        // labirinto, reaproveitando a mesma função usada pelos ratos.
        auto collidesAt = [&](const glm::vec3 &testPos)
        {
            return MazeCollides(testPos.x, testPos.z, g_PlayerHalfWidth, g_PlayerHalfDepth);
        };

        // Axis sliding: resolve X then Z so player can slide along walls.
        glm::vec3 tryPos = ConstrainPlayerToGround(prevPos);
        tryPos.x += moveDelta.x;
        if (!collidesAt(tryPos))
            prevPos.x = tryPos.x;

        tryPos = ConstrainPlayerToGround(prevPos);
        tryPos.z += moveDelta.z;
        if (!collidesAt(tryPos))
            prevPos.z = tryPos.z;

        g_PlayerPosition = ConstrainPlayerToGround(prevPos);

        // Avança a fase da animação de andar proporcionalmente à distância
        // realmente percorrida pelo jogador neste frame (no plano XZ, já
        // descontando colisões com paredes que tenham bloqueado o
        // movimento) - e não diretamente por "dt" - para que o ciclo da
        // passada acompanhe a velocidade real do personagem e fique
        // completamente parado quando o jogador não anda (incluindo quando
        // uma tecla é pressionada contra uma parede e o movimento é
        // bloqueado pela colisão).
        {
            glm::vec3 actualDelta = g_PlayerPosition - playerPosBeforeMove;
            float distanceMoved = glm::length(glm::vec2(actualDelta.x, actualDelta.z));
            g_PlayerWalkPhase += distanceMoved * g_PlayerWalkPhaseSpeed;
        }

        // Atualiza a simulação dos ratinhos (movimento aleatório via Bézier)
        // e testa a colisão cubo-esfera entre o jogador e o smile, além da
        // colisão cubo-cubo entre o jogador e os ratos (que os assusta e
        // faz fugir). Todas essas verificações só acontecem enquanto o jogo
        // está em andamento.
        if (g_GameState == GAME_PLAYING)
        {
            UpdateRats(dt);
            CheckPlayerRatCollisions();

            // Som ambiente: toca um guincho de rato (aleatório, com
            // intervalo mínimo entre repetições) sempre que pelo menos um
            // rato estiver visível para o jogador (dentro do campo de
            // visão da câmera em primeira pessoa, sem parede no meio).
            glm::vec3 playerEyePos = glm::vec3(
                g_PlayerPosition.x,
                g_PlayerPosition.y + g_PlayerEyeHeight,
                g_PlayerPosition.z);

            bool anyRatVisible = false;
            for (const Rat &rat : g_Rats)
            {
                if (IsRatVisibleToPlayer(rat, playerEyePos, front3))
                {
                    anyRatVisible = true;
                    break;
                }
            }
            Audio_UpdateRatSqueaks(dt, anyRatVisible);

            glm::vec3 playerBoxCenter = glm::vec3(
                g_PlayerPosition.x,
                g_PlayerPosition.y + g_PlayerHalfHeight,
                g_PlayerPosition.z);
            glm::vec3 playerHalfExtents = glm::vec3(g_PlayerHalfWidth, g_PlayerHalfHeight, g_PlayerHalfDepth);

            if (SphereAabbIntersect(g_SmileCenter, g_SmileRadius, playerBoxCenter, playerHalfExtents))
            {
                g_GameState = GAME_OVER;
                g_GameOverElapsed = currentTime - g_GameStartTime;

                // A trilha de fundo toca em loop "até o jogo acabar": agora
                // que o jogador venceu, ela para e o som de vitória toca.
                Audio_StopBackgroundMusic();
                Audio_PlayWinSound();

                // Libera o cursor do mouse (estava capturado pelo modo FPS)
                // para que o jogador consiga mover o mouse até o botão
                // "Jogar novamente" e clicar nele.
                if (g_FpsMode)
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        // Camera position and view
        glm::mat4 view;
        if (g_TopView)
        {
            glm::vec4 camera_position_c =
                glm::vec4(0.0f,
                          wallHeight * 8.0f,
                          0.0f,
                          1.0f);

            glm::vec4 camera_view_vector =
                glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);

            glm::vec4 camera_up_vector =
                glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

            view = Matrix_Camera_View(
                camera_position_c,
                camera_view_vector,
                camera_up_vector);
        }
        else
        {
            glm::vec4 camera_position_c =
                glm::vec4(g_PlayerPosition.x,
                          g_PlayerPosition.y + g_PlayerEyeHeight,
                          g_PlayerPosition.z,
                          1.0f);

            glm::vec4 camera_view_vector =
                glm::vec4(front3, 0.0f);

            glm::vec4 camera_up_vector =
                glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

            view = Matrix_Camera_View(
                camera_position_c,
                camera_view_vector,
                camera_up_vector);
        }

        // Agora computamos a matriz de Projeção.
        glm::mat4 projection;

        // Note que, no sistema de coordenadas da câmera, os planos near e far
        // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        float nearplane = -0.1f;  // Posição do "near plane"
        float farplane = -100.0f; // Posição do "far plane"

        if (g_UsePerspectiveProjection)
        {
            // Projeção Perspectiva.
            // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projeção Ortográfica.
            // Para definição dos valores l, r, b, t ("left", "right", "bottom", "top"),
            // PARA PROJEÇÃO ORTOGRÁFICA veja slides 219-224 do documento Aula_09_Projecoes.pdf.
            // Para simular um "zoom" ortográfico, computamos o valor de "t"
            // utilizando a variável g_CameraDistance.
            float t = 1.5f * g_CameraDistance / 2.5f;
            float b = -t;
            float r = t * g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

        // A fonte de luz principal da cena é a lanterna do personagem. Sua
        // posição (mundo) é recalculada a cada frame a partir da posição e
        // orientação atuais do jogador, e enviada ao shader (usada para o
        // termo difuso/especular com atenuação por distância). A visibilidade
        // da luz para cada objeto (g_light_visibility_uniform, um valor
        // contínuo entre 0 e 1, não um booleano) é calculada individualmente
        // antes de cada DrawVirtualObject(), mais abaixo, suavizando a
        // transição com a sombra das paredes do labirinto.
        glm::vec3 lanternPos = ComputeLanternPosition();
        glUniform4f(g_light_position_uniform, lanternPos.x, lanternPos.y, lanternPos.z, 1.0f);

        // Direção da lanterna (spotlight): para onde o personagem está
        // olhando agora (yaw + pitch). Usada no fragment shader para
        // restringir a luz a um CONE na frente do personagem, em vez de
        // espalhar igualmente em todas as direções a partir da posição da
        // lanterna (o que fazia a luz parecer uma lâmpada onidirecional
        // flutuando ao lado do personagem, e não uma lanterna apontada para
        // frente).
        glm::vec3 lanternDir = ComputeLanternDirection();
        glUniform4f(g_light_direction_uniform, lanternDir.x, lanternDir.y, lanternDir.z, 0.0f);

#define SPHERE 0
#define PLANE 2
#define WALL 3
#define TETO 4
#define RAT 5
#define WALL_GLOBE 6

        float mazeSizeX = mazeW * cellSize;
        float mazeSizeZ = mazeH * cellSize;

        // Chão: desenhado CÉLULA A CÉLULA (em vez de um único quad gigante
        // cobrindo o labirinto inteiro) usando o mesmo "floor_tile" (cubo
        // fino) para todas as células, justamente para podermos calcular e
        // vetorizar por vértice a visibilidade da luz da lanterna também no
        // piso - testando oclusão por parede em cada um dos 4 cantos de
        // CADA célula. Isso corrige a luz "vazando" por baixo das paredes
        // (antes o chão usava sempre light_visibility = 1.0, sem nenhum
        // teste de oclusão, então a luz da lanterna atravessava paredes no
        // plano do piso).
        glUniform1i(g_object_id_uniform, PLANE);

        if (g_texture_repeat_uniform != -1)
            glUniform1f(g_texture_repeat_uniform, 1.0f);

        // Altura bem pequena para o "ladrilho" parecer um plano fino, e não
        // um cubo visível (sua face de cima é a única que realmente importa
        // visualmente; o resto fica abaixo do nível do piso/oculto).
        const float kFloorTileThickness = 0.02f;

        for (int i = 0; i < mazeH; i++)
        {
            for (int j = 0; j < mazeW; j++)
            {
                float x = (j - mazeW / 2.0f + 0.5f) * cellSize;
                float z = (i - mazeH / 2.0f + 0.5f) * cellSize;

                glm::mat4 tileModel =
                    Matrix_Translate(x, g_GroundY - kFloorTileThickness * 0.5f, z) *
                    Matrix_Scale(cellSize, kFloorTileThickness, cellSize);

                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(tileModel));

                // Mantém um piso de iluminação ambiente baixo via o uniform
                // "de objeto" (igual ao comportamento anterior), e refina
                // com o peso por vértice calculado abaixo (oclusão real por
                // parede em cada canto da célula).
                glUniform1f(g_light_visibility_uniform, 1.0f);
                UpdateVertexLightWeights("floor_tile", tileModel, lanternPos);

                DrawVirtualObject("floor_tile");
            }
        }

        // Esfera dentro do labirinto (objetivo do jogador). A hitbox desta
        // esfera é usada para a colisão jogador-smile (cubo-esfera).
        float sphereX = (g_SphereCol - mazeW / 2.0f + 0.5f) * cellSize;
        float sphereZ = (g_SphereRow - mazeH / 2.0f + 0.5f) * cellSize;

        float sphereScale = 0.2f * cellSize;

        // A malha "the_sphere" (data/sphere.obj) tem raio unitário (1.0) no
        // espaço do modelo, logo o raio em coordenadas de mundo é igual ao
        // fator de escala uniforme aplicado abaixo.
        g_SmileCenter = glm::vec3(sphereX, g_GroundY + 0.4f, sphereZ);
        g_SmileRadius = sphereScale;

        model =
            Matrix_Translate(
                sphereX,
                g_GroundY + 0.4f,
                sphereZ) *
            Matrix_Rotate_Y((float)glfwGetTime()) * Matrix_Scale(sphereScale, sphereScale, sphereScale);

        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SPHERE);
        glUniform1f(g_light_visibility_uniform, ComputeLightVisibility(lanternPos, g_SmileCenter, sphereScale));

        DrawVirtualObject("the_sphere");

        // Teto
        if (!g_TopView)
        {
            glDisable(GL_CULL_FACE);

            model = Matrix_Translate(0.0f, g_GroundY + wallHeight, 0.0f) * Matrix_Rotate_X(3.1415926f) * Matrix_Scale(mazeSizeX, 1.0f, mazeSizeZ);

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, TETO);
            glUniform1f(g_light_visibility_uniform, 1.0f);

            if (g_texture_repeat_uniform != -1)
                glUniform1f(g_texture_repeat_uniform, mazeW);

            DrawVirtualObject("the_plane");

            glEnable(GL_CULL_FACE);
        }

        // Paredes do labirinto (textura "brick" ou "globo"): cada parede
        // usa exatamente UMA cópia completa da textura por face (sem
        // repetição/tiling), preenchendo o bloco inteiro com uma única
        // imagem - por isso TextureRepeat = 1.0 aqui. Antes este valor
        // ficava em 2.0 (resíduo de um bloco de código morto que nunca
        // chegava a desenhar nada), o que fazia a textura repetir 2x em
        // cada eixo da face (4 cópias visíveis por parede).
        glUniform1i(g_object_id_uniform, WALL);
        if (g_texture_repeat_uniform != -1)
            glUniform1f(g_texture_repeat_uniform, 1.0f);
        // horizontal walls
        for (int i = 0; i <= mazeH; i++)
        {
            for (int j = 0; j < mazeW; j++)
            {
                if (wallHorz[i][j])
                {
                    if (g_OnlyBorderWalls && !(i == 0 || i == mazeH))
                        continue;

                    float x = (j - mazeW / 2.0f + 0.5f) * cellSize;
                    float z = (i - mazeH / 2.0f) * cellSize;

                    glm::mat4 wm =
                        Matrix_Translate(x, g_GroundY + wallHeight / 2.0f, z) * Matrix_Scale(cellSize, wallHeight, wallThickness);

                    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(wm));

                    // Cada parede individualmente sorteada como "globo" (ver
                    // AssignWallTextures() em maze.cpp) usa a textura de
                    // globo (TextureImage5) em vez da textura padrão de
                    // tijolos; as demais continuam usando WALL normalmente.
                    glUniform1i(g_object_id_uniform, wallHorzIsGlobe[i][j] ? WALL_GLOBE : WALL);

                    glm::vec3 wallCenter = glm::vec3(x, g_GroundY + wallHeight / 2.0f, z);
                    glUniform1f(g_light_visibility_uniform, ComputeLightVisibility(lanternPos, wallCenter, cellSize * 0.5f));

                    // Vetorização da iluminação por vértice: recalcula a
                    // visibilidade da luz em cada um dos 8 cantos desta
                    // parede (em vez de um único valor para a parede
                    // inteira), suavizando a transição da sombra ao longo
                    // da própria face quando ela está parcialmente ocluída.
                    UpdateVertexLightWeights("wall_cube", wm, lanternPos);

                    DrawVirtualObject("wall_cube");
                }
            }
        }

        // vertical walls
        for (int i = 0; i < mazeH; i++)
        {
            for (int j = 0; j <= mazeW; j++)
            {
                if (wallVert[i][j])
                {
                    if (g_OnlyBorderWalls && !(j == 0 || j == mazeW))
                        continue;

                    float x = (j - mazeW / 2.0f) * cellSize;
                    float z = (i - mazeH / 2.0f + 0.5f) * cellSize;

                    glm::mat4 wm =
                        Matrix_Translate(x, g_GroundY + wallHeight / 2.0f, z) * Matrix_Scale(wallThickness, wallHeight, cellSize);

                    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(wm));

                    // Mesma lógica de escolha de textura usada nas paredes
                    // horizontais acima, aplicada agora às paredes verticais.
                    glUniform1i(g_object_id_uniform, wallVertIsGlobe[i][j] ? WALL_GLOBE : WALL);

                    glm::vec3 wallCenter = glm::vec3(x, g_GroundY + wallHeight / 2.0f, z);
                    glUniform1f(g_light_visibility_uniform, ComputeLightVisibility(lanternPos, wallCenter, cellSize * 0.5f));

                    // Mesma vetorização por vértice aplicada acima às
                    // paredes horizontais, agora para as paredes verticais.
                    UpdateVertexLightWeights("wall_cube", wm, lanternPos);

                    DrawVirtualObject("wall_cube");
                }
            }
        }

        // Ratinhos: desenhamos cada um na sua posição/orientação atual.
        glUniform1i(g_object_id_uniform, RAT);
        if (g_texture_repeat_uniform != -1)
            glUniform1f(g_texture_repeat_uniform, 1.0f);

        for (size_t r = 0; r < g_Rats.size(); r++)
        {
            const Rat &rat = g_Rats[r];

            model =
                Matrix_Translate(rat.position.x, g_GroundY, rat.position.z) *
                Matrix_Rotate_Y(rat.yaw) *
                Matrix_Scale(g_RatScale, g_RatScale, g_RatScale);

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(g_light_visibility_uniform, ComputeLightVisibility(lanternPos, rat.position, g_RatHalfSize));
            DrawVirtualObject("the_rat");
        }

        // O personagem do jogador (boneco explorador) é desenhado na visão
        // de cima (TAB), no lugar onde antes havia um marcador esférico
        // (removido, para não ser confundido com o "smile"). Em primeira
        // pessoa o boneco não é desenhado, já que a câmera fica dentro da
        // própria cabeça do personagem (não há, por ora, um "view model").
        if (g_TopView)
        {
            DrawPlayerCharacter(g_PlayerPosition, g_PlayerYaw, view, projection);
            // Restaura o programa de GPU principal da cena 3D (o personagem
            // usa seu próprio programa de shader, com cor sólida em vez de
            // textura) antes de continuar o resto do frame.
            glUseProgram(g_GpuProgramID);
        }

        glEnable(GL_CULL_FACE);

        // restore plane tiling for next frame
        if (g_texture_repeat_uniform != -1)
            glUniform1f(g_texture_repeat_uniform, 10.0f);
        // O jogador agora é somente uma hitbox cúbica invisível para colisão.

        // Imprimimos na tela os ângulos de Euler que controlam a rotação do
        // terceiro cubo.
        TextRendering_ShowEulerAngles(window);

        // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
        TextRendering_ShowProjection(window);

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        // Se o jogador encostou no smile, mostramos a tela de fim de jogo
        // (tempo de conclusão + botão "Jogar novamente") por cima de tudo.
        if (g_GameState == GAME_OVER)
        {
            DrawGameOverScreen(window, g_GameOverElapsed);
        }

        // Antes da primeira partida começar, mostramos a tela de menu
        // (objetivo, controles, caixa de seleção de música e botão
        // "Iniciar") por cima da cena 3D, que continua sendo desenhada
        // normalmente ao fundo (labirinto já sorteado acima, mas o
        // cronômetro/ratos/música ainda não foram iniciados).
        if (g_GameState == GAME_MENU)
        {
            DrawMenuScreen(window);
        }

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Libera os recursos do sistema de áudio (engine, sons carregados).
    Audio_Shutdown();

    // Fim do programa
    return 0;
}

// Implementation of BuildCubeAndAddToVirtualScene (placed after SceneObject definition)
//
// Constrói um cubo com cada uma de suas 6 faces SUBDIVIDIDA em uma grade de
// "kGridSubdivisions x kGridSubdivisions" quads (em vez de um único quad/2
// triângulos por face, como antes). Mais vértices por face significam mais
// pontos de amostra para a vetorização da iluminação por vértice
// (UpdateVertexLightWeights(), em main.cpp): com apenas os 4 cantos de uma
// face grande (como a face frontal de uma parede comprida), a sombra
// interpolada entre eles tende a aparecer "poligonal"/quadrada perto de
// bordas de oclusão; com uma grade mais fina, a mesma sombra é amostrada e
// interpolada em muito mais pontos ao longo da face, aproximando-se de uma
// transição suave e contínua.
void BuildCubeAndAddToVirtualScene(float size, const char *name)
{
    float h = size / 2.0f;

    // Número de subdivisões por lado de cada face (grade
    // kGridSubdivisions x kGridSubdivisions de quads, ou seja,
    // (kGridSubdivisions+1) x (kGridSubdivisions+1) vértices por face).
    // Valor escolhido como equilíbrio entre suavidade da sombra vetorizada
    // por vértice e custo de CPU (UpdateVertexLightWeights testa oclusão
    // por parede individualmente para cada vértice, a cada parede, a cada
    // frame).
    const int kGridSubdivisions = 4;

    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;
    std::vector<GLuint> indices;

    // Emite uma face plana subdividida em grade, com canto inferior-esquerdo
    // "origin", e dois vetores de borda "edgeU"/"edgeV" (cada um already
    // escalado para o tamanho TOTAL da face); "normal" é a normal da face
    // (constante para todos os vértices dela, já que é plana).
    auto emitGridFace = [&](glm::vec3 origin, glm::vec3 edgeU, glm::vec3 edgeV, glm::vec3 normal)
    {
        GLuint baseIndex = (GLuint)(model_coefficients.size() / 4);

        for (int row = 0; row <= kGridSubdivisions; row++)
        {
            float tV = (float)row / (float)kGridSubdivisions;
            for (int col = 0; col <= kGridSubdivisions; col++)
            {
                float tU = (float)col / (float)kGridSubdivisions;

                glm::vec3 pos = origin + edgeU * tU + edgeV * tV;

                model_coefficients.push_back(pos.x);
                model_coefficients.push_back(pos.y);
                model_coefficients.push_back(pos.z);
                model_coefficients.push_back(1.0f);

                normal_coefficients.push_back(normal.x);
                normal_coefficients.push_back(normal.y);
                normal_coefficients.push_back(normal.z);
                normal_coefficients.push_back(0.0f);

                // Coordenadas de textura: a face inteira (0..1 em ambos os
                // eixos) continua mapeando exatamente UMA cópia completa da
                // textura, exatamente como antes da subdivisão - só a
                // densidade de vértices aumentou, não a repetição (tiling)
                // da imagem.
                texture_coefficients.push_back(tU);
                texture_coefficients.push_back(tV);
            }
        }

        int stride = kGridSubdivisions + 1;
        for (int row = 0; row < kGridSubdivisions; row++)
        {
            for (int col = 0; col < kGridSubdivisions; col++)
            {
                GLuint i0 = baseIndex + row * stride + col;
                GLuint i1 = baseIndex + row * stride + col + 1;
                GLuint i2 = baseIndex + (row + 1) * stride + col + 1;
                GLuint i3 = baseIndex + (row + 1) * stride + col;

                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);

                indices.push_back(i2);
                indices.push_back(i3);
                indices.push_back(i0);
            }
        }
    };

    // As 6 faces, cada uma definida por um canto de origem + dois vetores de
    // borda (U e V) que varrem o tamanho total da face - preservando
    // exatamente a mesma posição, orientação de normal e sentido de
    // enrolamento (winding order / CCW visto de fora) que a versão anterior
    // (não subdividida) desta mesma função.
    emitGridFace(glm::vec3(-h, -h, h), glm::vec3(2 * h, 0, 0), glm::vec3(0, 2 * h, 0), glm::vec3(0, 0, 1));  // front
    emitGridFace(glm::vec3(h, -h, -h), glm::vec3(-2 * h, 0, 0), glm::vec3(0, 2 * h, 0), glm::vec3(0, 0, -1)); // back
    emitGridFace(glm::vec3(-h, -h, -h), glm::vec3(0, 0, 2 * h), glm::vec3(0, 2 * h, 0), glm::vec3(-1, 0, 0)); // left
    emitGridFace(glm::vec3(h, -h, h), glm::vec3(0, 0, -2 * h), glm::vec3(0, 2 * h, 0), glm::vec3(1, 0, 0));   // right
    emitGridFace(glm::vec3(-h, h, h), glm::vec3(2 * h, 0, 0), glm::vec3(0, 0, -2 * h), glm::vec3(0, 1, 0));   // top
    emitGridFace(glm::vec3(-h, -h, -h), glm::vec3(2 * h, 0, 0), glm::vec3(0, 0, 2 * h), glm::vec3(0, -1, 0)); // bottom

    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW);
    GLuint location = 0;
    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);

    GLuint VBO_normal_coefficients_id;
    glGenBuffers(1, &VBO_normal_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW);
    location = 1;
    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);

    GLuint VBO_texture_coefficients_id;
    glGenBuffers(1, &VBO_texture_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW);
    location = 2;
    glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);

    // VBO dinâmico (location 3) com o peso de visibilidade de luz por
    // vértice: começa todo em 1.0 (totalmente iluminado) e é sobrescrito a
    // cada frame, por instância de parede desenhada, em
    // UpdateVertexLightWeights() - permitindo que a sombra se espalhe
    // suavemente AO LONGO da própria face da parede (vetorização da
    // iluminação por vértice / shading tipo Gouraud para a visibilidade),
    // em vez de um valor único e abrupto por parede. Com a face agora
    // subdividida em grade, este peso é calculado em MUITO mais pontos por
    // face do que antes (só os 4 cantos), suavizando bastante a transição.
    size_t numVertices = model_coefficients.size() / 4;
    std::vector<float> initialLightWeights(numVertices, 1.0f);

    GLuint VBO_light_weight_id;
    glGenBuffers(1, &VBO_light_weight_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_light_weight_id);
    glBufferData(GL_ARRAY_BUFFER, initialLightWeights.size() * sizeof(float), initialLightWeights.data(), GL_DYNAMIC_DRAW);
    location = 3;
    glVertexAttribPointer(location, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);

    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    SceneObject theobject;
    theobject.name = name;
    theobject.first_index = 0;
    theobject.num_indices = indices.size();
    theobject.rendering_mode = GL_TRIANGLES;
    theobject.vertex_array_object_id = vertex_array_object_id;
    theobject.bbox_min = glm::vec3(-h, -h, -h);
    theobject.bbox_max = glm::vec3(h, h, h);
    theobject.vertex_light_weight_vbo_id = VBO_light_weight_id;
    theobject.num_vertices = numVertices;
    g_VirtualScene[name] = theobject;

    // Guarda também as posições locais (x,y,z) de cada vértice do cubo, na
    // mesma ordem em que foram emitidas acima, para que
    // UpdateVertexLightWeights() saiba calcular o peso de cada vértice a
    // partir da sua posição local (canto "esquerdo" ou "direito" da parede
    // ao longo do seu eixo mais longo).
    g_CubeLocalVertexPositions[name].clear();
    for (size_t k = 0; k < numVertices; k++)
    {
        g_CubeLocalVertexPositions[name].push_back(glm::vec3(
            model_coefficients[k * 4 + 0],
            model_coefficients[k * 4 + 1],
            model_coefficients[k * 4 + 2]));
    }
}

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char *filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if (data == NULL)
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slides 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
    // Para permitir repetição (tiling) da textura no plano, usamos REPEAT.
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Parâmetros de amostragem da textura.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char *object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void *)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    GLuint vertex_shader_id =
        LoadShader_Vertex(FindFile("src/shader_vertex.glsl").c_str());

    GLuint fragment_shader_id =
        LoadShader_Fragment(FindFile("src/shader_fragment.glsl").c_str());

    // Deletamos o programa de GPU anterior, caso ele exista.
    if (g_GpuProgramID != 0)
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform = glGetUniformLocation(g_GpuProgramID, "model");           // Variável da matriz "model"
    g_view_uniform = glGetUniformLocation(g_GpuProgramID, "view");             // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_object_id_uniform = glGetUniformLocation(g_GpuProgramID, "object_id");   // Variável "object_id" em shader_fragment.glsl
    g_bbox_min_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_light_position_uniform = glGetUniformLocation(g_GpuProgramID, "light_position");
    g_light_direction_uniform = glGetUniformLocation(g_GpuProgramID, "light_direction");
    g_light_visibility_uniform = glGetUniformLocation(g_GpuProgramID, "light_visibility");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage4"), 4);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage5"), 5);
    g_texture_repeat_uniform = glGetUniformLocation(g_GpuProgramID, "TextureRepeat");
    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4 &M)
{
    if (g_MatrixStack.empty())
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel *model)
{
    if (!model->attrib.normals.empty())
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice e que pertencem ao mesmo "smoothing group".

    // Obtemos a lista dos smoothing groups que existem no objeto
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        assert(model->shapes[shape].mesh.smoothing_group_ids.size() == num_triangles);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            assert(sgroup >= 0);
            sgroup_ids.insert(sgroup);
        }
    }

    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve(3 * num_vertices);

    // Processamos um smoothing group por vez
    for (const unsigned int &sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

        // Acumulamos as normais dos vértices de todos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                glm::vec4 vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    const float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
                    const float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
                    const float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
                    vertices[vertex] = glm::vec4(vx, vy, vz, 1.0);
                }

                const glm::vec4 a = vertices[0];
                const glm::vec4 b = vertices[1];
                const glm::vec4 c = vertices[2];

                const glm::vec4 n = crossproduct(b - a, c - a);

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }

        // Computamos a média das normais acumuladas
        std::vector<size_t> normal_indices(num_vertices, 0);

        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index)
        {
            if (num_triangles_per_vertex[vertex_index] == 0)
                continue;

            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index];
            n /= norm(n);

            model->attrib.normals.push_back(n.x);
            model->attrib.normals.push_back(n.y);
            model->attrib.normals.push_back(n.z);

            size_t normal_index = (model->attrib.normals.size() / 3) - 1;
            normal_indices[vertex_index] = normal_index;
        }

        // Escrevemos os índices das normais para os vértices dos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    model->shapes[shape].mesh.indices[3 * triangle + vertex].normal_index =
                        normal_indices[idx.vertex_index];
                }
            }
        }
    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel *model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        // Em 2026-05-18, corrigido bug encontrado pelo aluno Arthur Prediger:
        // std::numeric_limits<float>::min() retorna o menor valor positivo
        // normalizado representável, não o menor valor possível (negativo). Para
        // inicializar o limite máximo da bounding box com um valor "muito
        // pequeno", deve ser usado std::numeric_limits<float>::lowest()
        const float minval = std::numeric_limits<float>::lowest();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval, maxval, maxval);
        glm::vec3 bbox_max = glm::vec3(minval, minval, minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];

                indices.push_back(first_index + 3 * triangle + vertex);

                const float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
                // printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back(vx);   // X
                model_coefficients.push_back(vy);   // Y
                model_coefficients.push_back(vz);   // Z
                model_coefficients.push_back(1.0f); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if (idx.normal_index != -1)
                {
                    const float nx = model->attrib.normals[3 * idx.normal_index + 0];
                    const float ny = model->attrib.normals[3 * idx.normal_index + 1];
                    const float nz = model->attrib.normals[3 * idx.normal_index + 2];
                    normal_coefficients.push_back(nx);   // X
                    normal_coefficients.push_back(ny);   // Y
                    normal_coefficients.push_back(nz);   // Z
                    normal_coefficients.push_back(0.0f); // W
                }

                if (idx.texcoord_index != -1)
                {
                    const float u = model->attrib.texcoords[2 * idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2 * idx.texcoord_index + 1];
                    texture_coefficients.push_back(u);
                    texture_coefficients.push_back(v);
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name = model->shapes[shape].name;
        theobject.first_index = first_index;                  // Primeiro índice
        theobject.num_indices = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;              // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0;            // "(location = 0)" em "shader_vertex.glsl"
    GLint number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!normal_coefficients.empty())
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1;             // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (!texture_coefficients.empty())
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2;             // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// A hitbox do jogador é um cubo invisível apoiado no plano do chão.
// A posição g_PlayerPosition representa os pés do jogador no centro do mapa.
glm::vec3 ConstrainPlayerToGround(glm::vec3 candidate_position)
{
    candidate_position.y = g_GroundY;
    return candidate_position;
}

// ===========================================================================
// NOSSAS FUNÇÕES: ratinhos, colisão jogador-smile e tela de fim de jogo.
// ===========================================================================

// Gerador de números aleatórios global, usado para o spawn e o movimento dos
// ratinhos. Semeado uma única vez com a hora atual.
static std::mt19937 g_RatRng((unsigned int)std::time(nullptr));

// Retorna a posição (mundo) do centro da esfera "smile", consistente com a
// posição usada no laço de renderização em main().
glm::vec3 ComputeSmilePosition()
{
    float sphereX = (g_SphereCol - mazeW / 2.0f + 0.5f) * cellSize;
    float sphereZ = (g_SphereRow - mazeH / 2.0f + 0.5f) * cellSize;
    return glm::vec3(sphereX, g_GroundY + 0.4f, sphereZ);
}

// Retorna a posição (mundo) da lanterna do personagem: a fonte de luz
// principal da cena em primeira pessoa. Calculada a partir da posição e
// orientação atuais do jogador, com um pequeno deslocamento para a frente
// e para a direita (simulando a mão direita estendida), na altura
// aproximada do peito/mão.
glm::vec3 ComputeLanternPosition()
{
    glm::vec3 forward = glm::vec3(sinf(g_PlayerYaw), 0.0f, cosf(g_PlayerYaw));
    glm::vec3 right = glm::vec3(cosf(g_PlayerYaw), 0.0f, -sinf(g_PlayerYaw));

    float forwardOffset = 0.35f;
    float rightOffset = 0.22f;
    float lanternHeight = g_GroundY + g_PlayerEyeHeight * 0.75f;

    return g_PlayerPosition + forward * forwardOffset + right * rightOffset + glm::vec3(0.0f, lanternHeight - g_PlayerPosition.y, 0.0f);
}

// Direção (mundo, normalizada) para onde a lanterna aponta: segue o olhar
// completo do jogador (yaw E pitch, exatamente a mesma fórmula usada para o
// vetor "front" da câmera em primeira pessoa), e não só o yaw (rotação
// horizontal) como antes. Isso é o que faz a luz se comportar como uma
// LANTERNA de fato - apontando para onde o personagem está olhando, inclusive
// para cima/baixo - em vez de uma luz pontual onidirecional que ilumina
// igualmente em todas as direções a partir da posição da lanterna.
glm::vec3 ComputeLanternDirection()
{
    glm::vec3 dir;
    dir.x = cosf(g_PlayerPitch) * sinf(g_PlayerYaw);
    dir.y = sinf(g_PlayerPitch);
    dir.z = cosf(g_PlayerPitch) * cosf(g_PlayerYaw);
    return glm::normalize(dir);
}

// Testa se a luz da lanterna alcança um ponto específico do mundo em linha
// reta, amostrando pontos ao longo do segmento lanterna->ponto e
// verificando se algum deles colide com uma parede do labirinto (mesma
// técnica usada em IsRatVisibleToPlayer, reaproveitando MazeCollides).
static bool LightReachesPointExact(glm::vec3 lightPos, glm::vec3 targetPos)
{
    glm::vec3 toTarget = targetPos - lightPos;
    toTarget.y = 0.0f;

    float distance = glm::length(toTarget);
    if (distance < 1e-4f)
        return true;

    const float kStepSize = 0.12f;
    int numSteps = (int)(distance / kStepSize);
    numSteps = glm::clamp(numSteps, 1, 200);

    for (int i = 1; i < numSteps; i++)
    {
        float t = (float)i / (float)numSteps;
        glm::vec3 samplePoint = lightPos + toTarget * t;
        if (MazeCollides(samplePoint.x, samplePoint.z, 0.02f, 0.02f))
            return false;
    }

    return true;
}

// Versão SUAVIZADA da checagem de oclusão por paredes: em vez de testar
// apenas o centro de um objeto (o que produziria uma transição abrupta -
// "tudo ou nada" - entre um objeto totalmente iluminado e o objeto vizinho
// totalmente na sombra), testamos vários pontos amostrados ao redor do
// centro (um pequeno "disco" de raio "sampleRadius", no plano XZ) e
// retornamos a FRAÇÃO desses pontos que a luz alcança, um valor contínuo
// entre 0.0 (totalmente na sombra) e 1.0 (totalmente iluminado). Perto da
// borda de uma sombra, parte das amostras "vê" a luz e parte não, dando
// uma transição suave e gradual em vez de um corte abrupto.
float ComputeLightVisibility(glm::vec3 lightPos, glm::vec3 targetPos, float sampleRadius)
{
    // Pontos de amostra: o próprio centro, mais 8 pontos ao redor dele
    // (cima/baixo/esquerda/direita e as 4 diagonais), formando uma
    // pequena grade 3x3 no plano XZ.
    const int kNumOffsets = 9;
    glm::vec3 offsets[kNumOffsets] = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.7071f, 0.0f, 0.7071f),
        glm::vec3(0.7071f, 0.0f, -0.7071f),
        glm::vec3(-0.7071f, 0.0f, 0.7071f),
        glm::vec3(-0.7071f, 0.0f, -0.7071f),
    };

    int visibleCount = 0;
    for (int i = 0; i < kNumOffsets; i++)
    {
        glm::vec3 sample = targetPos + offsets[i] * sampleRadius;
        if (LightReachesPointExact(lightPos, sample))
            visibleCount++;
    }

    return (float)visibleCount / (float)kNumOffsets;
}

// ---------------------------------------------------------------------------
// Vetorização da iluminação por vértice (sombras suaves ao longo da própria
// face de um objeto).
// ---------------------------------------------------------------------------
//
// Antes, a visibilidade da luz da lanterna (oclusão por paredes do
// labirinto) era um único valor "uniform" por chamada de desenho: a parede
// inteira recebia o MESMO peso de sombra, calculado em um único ponto
// (aproximadamente o centro da parede). Isso já evitava o corte abrupto
// "tudo ou nada" ENTRE paredes vizinhas (uma parede totalmente clara ao lado
// de outra totalmente escura), mas ainda produzia uma transição abrupta
// DENTRO da própria face de uma parede longa que estivesse parcialmente na
// sombra (ex.: metade dela ainda dentro do alcance da lanterna, metade já
// ocluída por uma parede adjacente).
//
// Esta função calcula a visibilidade da luz em CADA VÉRTICE da parede
// (reaproveitando LightReachesPointExact ponto a ponto, sem a amostragem em
// disco usada por ComputeLightVisibility, já que aqui cada vértice já É um
// ponto específico da geometria), e envia esses pesos individuais para a
// GPU através do VBO dinâmico de "light weight" (location 3 no vertex
// shader). O rasterizador então INTERPOLA esse peso entre os vértices de
// cada triângulo (exatamente como faz com cor/normais no Gouraud shading),
// produzindo uma transição suave e contínua da sombra ao longo da face,
// proporcional à distância de cada ponto até a "borda" da oclusão.
void UpdateVertexLightWeights(const char *object_name, const glm::mat4 &model, glm::vec3 lightPos)
{
    auto sceneIt = g_VirtualScene.find(object_name);
    auto localPosIt = g_CubeLocalVertexPositions.find(object_name);
    if (sceneIt == g_VirtualScene.end() || localPosIt == g_CubeLocalVertexPositions.end())
        return;

    SceneObject &obj = sceneIt->second;
    const std::vector<glm::vec3> &localPositions = localPosIt->second;

    if (obj.vertex_light_weight_vbo_id == 0 || localPositions.size() != obj.num_vertices)
        return;

    std::vector<float> weights(obj.num_vertices);
    for (size_t k = 0; k < obj.num_vertices; k++)
    {
        glm::vec4 worldPos4 = model * glm::vec4(localPositions[k], 1.0f);
        glm::vec3 worldPos = glm::vec3(worldPos4.x, worldPos4.y, worldPos4.z);
        weights[k] = LightReachesPointExact(lightPos, worldPos) ? 1.0f : 0.0f;
    }

    glBindBuffer(GL_ARRAY_BUFFER, obj.vertex_light_weight_vbo_id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, weights.size() * sizeof(float), weights.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Sorteia uma célula aleatória para o jogador nascer, regenera o labirinto
// inteiro a partir dessa célula (o algoritmo DFS usado em GenerateMaze
// produz um "labirinto perfeito" - uma árvore de expansão completa - o que
// garante, por construção, exatamente um caminho entre QUAISQUER duas
// células do grid) e então sorteia uma célula diferente da do jogador para
// posicionar o "smile". Por fim, atualiza g_PlayerPosition/g_PlayerYaw e
// g_SphereRow/g_SphereCol de acordo.
void RandomizeStartAndGoalCells()
{
    std::uniform_int_distribution<int> rowDist(0, mazeH - 1);
    std::uniform_int_distribution<int> colDist(0, mazeW - 1);

    int playerRow = rowDist(g_RatRng);
    int playerCol = colDist(g_RatRng);

    GenerateMaze(playerRow, playerCol);

    // Sorteia quais paredes deste novo labirinto recebem a textura "globo"
    // em vez da textura padrão "brick", respeitando a taxa de spawn e o
    // espaçamento mínimo configurados em maze.cpp (kGlobeWallSpawnRate,
    // kMinBrickWallsBetweenGlobeWalls).
    AssignWallTextures();

    // Sorteia a célula do smile, evitando repetir a célula do jogador.
    int smileRow, smileCol;
    do
    {
        smileRow = rowDist(g_RatRng);
        smileCol = colDist(g_RatRng);
    } while (smileRow == playerRow && smileCol == playerCol && mazeW * mazeH > 1);

    g_SphereRow = smileRow;
    g_SphereCol = smileCol;

    g_PlayerPosition = glm::vec3(
        (playerCol - mazeW / 2.0f + 0.5f) * cellSize,
        g_GroundY,
        (playerRow - mazeH / 2.0f + 0.5f) * cellSize);

    // Orienta o jogador para olhar, de forma aproximada, em direção ao
    // smile, evitando que ele já comece de costas/encostado em uma parede.
    //
    // Convenção de yaw da câmera (diferente da convenção usada para orientar
    // o modelo 3D do rato): com pitch=0, a direção para onde a câmera olha
    // é (sin(yaw), cos(yaw)) em (x,z) - veja o cálculo de "front3" no laço
    // principal. Logo, para olhar na direção "toGoal", o yaw correto é
    // atan2(toGoal.x, toGoal.z) (sem o sinal trocado que é usado para o
    // modelo do rato).
    glm::vec3 toGoal = ComputeSmilePosition() - g_PlayerPosition;
    if (glm::length(glm::vec2(toGoal.x, toGoal.z)) > 1e-5f)
        g_PlayerYaw = atan2f(toGoal.x, toGoal.z);
    g_PlayerPitch = 0.0f;
}

// Sorteia um ponto de mundo dentro dos limites do labirinto (não necessariamente
// livre de paredes -- a validação de colisão é feita por quem chama).
glm::vec3 RandomPointInsideMaze(std::mt19937 &rng)
{
    float margin = 0.3f; // mantém o ponto longe das bordas externas
    float halfX = (mazeW * cellSize) / 2.0f - margin;
    float halfZ = (mazeH * cellSize) / 2.0f - margin;

    std::uniform_real_distribution<float> distX(-halfX, halfX);
    std::uniform_real_distribution<float> distZ(-halfZ, halfZ);

    return glm::vec3(distX(rng), g_GroundY, distZ(rng));
}

// Sorteia um novo trecho de curva de Bézier cúbica para um rato: o ponto de
// partida (p0) é a posição atual do rato, e os demais pontos de controle
// (p1, p2, p3) são sorteados dentro do labirinto, repetindo o sorteio do
// ponto final (p3) caso ele caia em cima de uma parede (testado com a mesma
// hitbox quadrada usada para a colisão do próprio rato).
void PickNewBezierLeg(Rat &rat, std::mt19937 &rng)
{
    rat.p0 = rat.position;

    glm::vec3 target = rat.position;
    const int maxAttempts = 30;
    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        glm::vec3 candidate = RandomPointInsideMaze(rng);
        if (!MazeCollides(candidate.x, candidate.z, g_RatHalfSize, g_RatHalfSize))
        {
            target = candidate;
            break;
        }
    }

    // Pontos de controle intermediários: interpolados entre início e fim,
    // com um deslocamento lateral aleatório, dando uma trajetória curva (em
    // vez de uma linha reta) ao ratinho.
    glm::vec3 dir = target - rat.p0;
    glm::vec3 perp = glm::vec3(-dir.z, 0.0f, dir.x); // perpendicular no plano XZ
    float perpLen = glm::length(perp);
    if (perpLen > 1e-5f)
        perp /= perpLen;

    std::uniform_real_distribution<float> lateralDist(-0.4f, 0.4f);
    float lateral = lateralDist(rng);

    rat.p1 = rat.p0 + dir * 0.33f + perp * lateral;
    rat.p2 = rat.p0 + dir * 0.66f + perp * lateral;
    rat.p3 = target;

    // Mantém todos os pontos de controle no plano do chão (Y constante).
    rat.p0.y = rat.p1.y = rat.p2.y = rat.p3.y = g_GroundY;

    rat.t = 0.0f;

    std::uniform_real_distribution<float> durationDist(1.5f, 3.5f);
    rat.duration = durationDist(rng);
}

// Sorteia um trecho de Bézier de FUGA: o destino é escolhido preferindo
// pontos que aumentem a distância até "awayFrom" (tipicamente a posição do
// jogador no momento da colisão), dentro do labirinto e sem colidir com
// paredes. Usado quando o jogador encosta no rato (teste cubo-cubo), para
// fazer o rato "assustado" correr na direção contrária ao perigo.
void PickFleeBezierLeg(Rat &rat, std::mt19937 &rng, glm::vec3 awayFrom)
{
    rat.p0 = rat.position;

    glm::vec3 fleeDir = rat.position - awayFrom;
    float fleeDirLen = glm::length(glm::vec2(fleeDir.x, fleeDir.z));
    if (fleeDirLen > 1e-5f)
        fleeDir /= fleeDirLen;
    else
    {
        // Jogador e rato praticamente no mesmo ponto: foge em uma direção
        // aleatória qualquer.
        std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
        float angle = angleDist(rng);
        fleeDir = glm::vec3(sinf(angle), 0.0f, cosf(angle));
    }

    // Tenta vários candidatos dentro do labirinto, escolhendo o que está
    // mais alinhado com a direção de fuga (maior produto escalar com
    // "fleeDir") entre os que não colidem com paredes. Isso aproxima um
    // comportamento de "correr para longe" sem precisar resolver caminhos
    // dentro do labirinto (o que seria mais caro computacionalmente).
    glm::vec3 bestTarget = rat.position + fleeDir * (cellSize * 1.5f);
    float bestScore = -1e9f;
    bool foundValid = false;

    const int maxAttempts = 30;
    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        glm::vec3 candidate = RandomPointInsideMaze(rng);
        if (MazeCollides(candidate.x, candidate.z, g_RatHalfSize, g_RatHalfSize))
            continue;

        glm::vec3 candidateDir = candidate - rat.position;
        float candidateLen = glm::length(glm::vec2(candidateDir.x, candidateDir.z));
        if (candidateLen < 1e-5f)
            continue;

        float score = glm::dot(glm::vec2(candidateDir.x, candidateDir.z) / candidateLen,
                                glm::vec2(fleeDir.x, fleeDir.z));

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = candidate;
            foundValid = true;
        }
    }

    glm::vec3 target = foundValid ? bestTarget : rat.position;

    glm::vec3 dir = target - rat.p0;
    glm::vec3 perp = glm::vec3(-dir.z, 0.0f, dir.x);
    float perpLen = glm::length(perp);
    if (perpLen > 1e-5f)
        perp /= perpLen;

    std::uniform_real_distribution<float> lateralDist(-0.2f, 0.2f); // fuga mais "direta", menos sinuosa
    float lateral = lateralDist(rng);

    rat.p1 = rat.p0 + dir * 0.33f + perp * lateral;
    rat.p2 = rat.p0 + dir * 0.66f + perp * lateral;
    rat.p3 = target;

    rat.p0.y = rat.p1.y = rat.p2.y = rat.p3.y = g_GroundY;

    rat.t = 0.0f;

    // Foge mais rápido que o passeio aleatório normal (duration menor =
    // percorre a mesma curva em menos tempo = mais veloz).
    std::uniform_real_distribution<float> fleeDurationDist(0.5f, 1.0f);
    rat.duration = fleeDurationDist(rng);
}

// Cria (ou recria) os ratinhos do labirinto em posições aleatórias válidas
// (fora das paredes), cada um já com seu primeiro trecho de Bézier sorteado.
void SpawnRats()
{
    g_Rats.clear();
    g_Rats.reserve(g_NumRats);

    for (int i = 0; i < g_NumRats; i++)
    {
        Rat rat;

        // Sorteia uma posição inicial que não colida com paredes.
        glm::vec3 start = RandomPointInsideMaze(g_RatRng);
        const int maxAttempts = 50;
        for (int attempt = 0; attempt < maxAttempts; attempt++)
        {
            glm::vec3 candidate = RandomPointInsideMaze(g_RatRng);
            if (!MazeCollides(candidate.x, candidate.z, g_RatHalfSize, g_RatHalfSize))
            {
                start = candidate;
                break;
            }
        }

        rat.position = start;
        rat.yaw = 0.0f;
        PickNewBezierLeg(rat, g_RatRng); // usa rat.position como p0 e sorteia o resto

        g_Rats.push_back(rat);
    }
}

// Avalia um ponto sobre a curva de Bézier cúbica definida pelos pontos de
// controle p0..p3, no parâmetro t em [0,1].
static glm::vec3 EvalCubicBezier(const glm::vec3 &p0, const glm::vec3 &p1,
                                  const glm::vec3 &p2, const glm::vec3 &p3, float t)
{
    float u = 1.0f - t;
    return u * u * u * p0 +
           3.0f * u * u * t * p1 +
           3.0f * u * t * t * p2 +
           t * t * t * p3;
}

// Avança a simulação de todos os ratinhos em dt segundos: cada rato anda ao
// longo do trecho de Bézier atual; ao terminar o trecho, um novo é sorteado
// (um trecho de fuga, se o rato estiver "assustado"; um trecho de passeio
// aleatório, caso contrário). A movimentação resultante é então validada
// contra a colisão com as paredes do labirinto (hitbox quadrada); se a nova
// posição colidiria com uma parede, o rato simplesmente sorteia um novo
// destino imediatamente (na prática isso quase não ocorre, já que os
// próprios pontos de controle já são validados em PickNewBezierLeg /
// PickFleeBezierLeg, mas mantemos a checagem por robustez/segurança).
void UpdateRats(float dt)
{
    for (Rat &rat : g_Rats)
    {
        // Atualiza o temporizador de "assustado": ao zerar, o rato volta a
        // se mover normalmente (passeio aleatório) a partir do próximo
        // trecho de Bézier sorteado.
        if (rat.scared)
        {
            rat.scaredTimer -= dt;
            if (rat.scaredTimer <= 0.0f)
            {
                rat.scared = false;
                rat.scaredTimer = 0.0f;
            }
        }

        auto pickNextLeg = [&](Rat &r)
        {
            if (r.scared)
                PickFleeBezierLeg(r, g_RatRng, g_PlayerPosition);
            else
                PickNewBezierLeg(r, g_RatRng);
        };

        if (rat.duration <= 0.0f)
        {
            pickNextLeg(rat);
            continue;
        }

        float prevT = rat.t;
        rat.t += dt / rat.duration;

        if (rat.t >= 1.0f)
        {
            rat.position = rat.p3;
            pickNextLeg(rat);
            continue;
        }

        glm::vec3 newPos = EvalCubicBezier(rat.p0, rat.p1, rat.p2, rat.p3, rat.t);
        newPos.y = g_GroundY;

        // Colisão do rato apenas com as paredes do labirinto (hitbox quadrada).
        if (MazeCollides(newPos.x, newPos.z, g_RatHalfSize, g_RatHalfSize))
        {
            // Trecho atual ficou inválido (não deveria acontecer na prática):
            // aborta o trecho e sorteia outro a partir da posição anterior.
            pickNextLeg(rat);
            continue;
        }

        // Atualiza a orientação (yaw) do rato para ele "olhar" para onde anda.
        //
        // Observação sobre a convenção de ângulos usada aqui: com yaw = 0 o
        // modelo do rato (rat.obj) aponta a cabeça para -Z. A matriz
        // Matrix_Rotate_Y(yaw) leva a direção local (0,0,-1) para a direção
        // de mundo (-sin(yaw), -cos(yaw)) em (x,z). Portanto, para que o
        // rato olhe (e portanto ande de frente, não de costas) na direção
        // do deslocamento "delta", o yaw correto satisfaz
        // (-sin(yaw), -cos(yaw)) = delta_normalizado, isto é,
        // yaw = atan2(-delta.x, -delta.z).
        glm::vec3 delta = newPos - rat.position;
        if (glm::length(glm::vec2(delta.x, delta.z)) > 1e-5f)
            rat.yaw = atan2f(-delta.x, -delta.z);

        rat.position = newPos;
        (void)prevT;
    }
}

// Testa se um rato está "à vista" do jogador: dentro de uma distância
// razoável, dentro do campo de visão da câmera em primeira pessoa (cone
// cujo ângulo de abertura é um pouco maior que o FOV da câmera, para soar
// mais perceptível), e sem nenhuma parede do labirinto bloqueando a linha
// reta entre o jogador e o rato. Usada apenas para decidir quando tocar os
// efeitos sonoros de guincho dos ratos (ver Audio_UpdateRatSqueaks).
bool IsRatVisibleToPlayer(const Rat &rat, glm::vec3 playerEyePos, glm::vec3 viewDir)
{
    const float kMaxVisibleDistance = cellSize * 5.0f; // ratos muito distantes não "contam"
    const float kHalfFovRadians = 0.6981317f;          // ~40 graus (um pouco mais aberto que o FOV de 30 graus da câmera)

    glm::vec3 toRat = rat.position - playerEyePos;
    toRat.y = 0.0f; // comparação só no plano horizontal (XZ)

    float distance = glm::length(toRat);
    if (distance < 1e-4f)
        return true; // rato exatamente na posição do jogador (caso degenerado)

    if (distance > kMaxVisibleDistance)
        return false;

    glm::vec3 toRatDir = toRat / distance;
    glm::vec3 viewDirXZ = glm::normalize(glm::vec3(viewDir.x, 0.0f, viewDir.z));

    float cosAngle = glm::dot(viewDirXZ, toRatDir);
    float cosHalfFov = cosf(kHalfFovRadians);
    if (cosAngle < cosHalfFov)
        return false; // fora do cone de visão

    // Verifica se há alguma parede bloqueando a linha de visão, amostrando
    // pontos ao longo do segmento jogador->rato e testando cada um contra
    // MazeCollides (a mesma função usada para a colisão de movimento). Uma
    // amostragem com passo pequeno e fixo é suficiente aqui, já que esta
    // checagem não precisa de precisão geométrica exata - é só para decidir
    // se toca ou não um efeito sonoro.
    const float kStepSize = 0.12f;
    int numSteps = (int)(distance / kStepSize);
    numSteps = glm::clamp(numSteps, 1, 200);

    for (int i = 1; i < numSteps; i++)
    {
        float t = (float)i / (float)numSteps;
        glm::vec3 samplePoint = playerEyePos + toRat * t;
        // Usa uma hitbox pontual (semi-largura quase zero) só para testar
        // se o próprio ponto está dentro de uma parede.
        if (MazeCollides(samplePoint.x, samplePoint.z, 0.02f, 0.02f))
            return false;
    }

    return true;
}

// Testa a colisão cubo-cubo entre o jogador e cada rato (hitbox cúbica do
// jogador vs. hitbox quadrada/cúbica do rato). Ao colidir, o rato entra em
// estado de "assustado": interrompe imediatamente o trecho de Bézier atual
// e começa a fugir na direção oposta ao jogador (ver PickFleeBezierLeg),
// permanecendo em fuga por g_RatScaredDuration segundos.
void CheckPlayerRatCollisions()
{
    glm::vec3 playerBoxCenter = glm::vec3(
        g_PlayerPosition.x,
        g_PlayerPosition.y + g_PlayerHalfHeight,
        g_PlayerPosition.z);
    glm::vec3 playerHalfExtents = glm::vec3(g_PlayerHalfWidth, g_PlayerHalfHeight, g_PlayerHalfDepth);

    // Hitbox do rato como um cubo: mesma semi-largura/profundidade usada
    // para a colisão com paredes (g_RatHalfSize), com uma altura pequena e
    // fixa (o modelo do rato é baixo), centrada um pouco acima do chão.
    glm::vec3 ratHalfExtents = glm::vec3(g_RatHalfSize, g_RatHalfSize * 0.6f, g_RatHalfSize);

    for (Rat &rat : g_Rats)
    {
        glm::vec3 ratBoxCenter = glm::vec3(rat.position.x, rat.position.y + ratHalfExtents.y, rat.position.z);

        if (AabbAabbIntersect(playerBoxCenter, playerHalfExtents, ratBoxCenter, ratHalfExtents))
        {
            // Toca o som de "rato assustado" apenas na transição para o
            // estado de fuga (e não em todo frame em que as hitboxes
            // continuarem sobrepostas), para não repetir o som sem parar.
            if (!rat.scared)
                Audio_PlayRatScaredSound();

            rat.scared = true;
            rat.scaredTimer = g_RatScaredDuration;
            // Interrompe o trecho atual e começa a fugir imediatamente, a
            // partir da posição atual do rato, na direção oposta ao jogador.
            PickFleeBezierLeg(rat, g_RatRng, g_PlayerPosition);
        }
    }
}

// Teste de intersecção entre uma esfera (centro + raio) e uma caixa alinhada
// aos eixos (AABB, centro + semi-extensões): usa a técnica clássica de
// encontrar o ponto da AABB mais próximo do centro da esfera e comparar a
// distância (ao quadrado) com o raio (ao quadrado) da esfera.
bool SphereAabbIntersect(glm::vec3 sphereCenter, float sphereRadius,
                          glm::vec3 boxCenter, glm::vec3 halfExtents)
{
    glm::vec3 boxMin = boxCenter - halfExtents;
    glm::vec3 boxMax = boxCenter + halfExtents;

    glm::vec3 closest = glm::clamp(sphereCenter, boxMin, boxMax);
    glm::vec3 diff = sphereCenter - closest;

    float distSq = glm::dot(diff, diff);
    return distSq <= (sphereRadius * sphereRadius);
}

// Teste de intersecção entre duas caixas alinhadas aos eixos (AABB-AABB),
// cada uma definida por centro + semi-extensões. Não está em uso pela lógica
// de jogo atual (ratos colidem apenas com paredes, por ora), mas fica
// disponível para uma futura colisão jogador-rato ou rato-rato.
bool AabbAabbIntersect(glm::vec3 centerA, glm::vec3 halfA,
                        glm::vec3 centerB, glm::vec3 halfB)
{
    glm::vec3 minA = centerA - halfA, maxA = centerA + halfA;
    glm::vec3 minB = centerB - halfB, maxB = centerB + halfB;

    return (minA.x <= maxB.x && maxA.x >= minB.x) &&
           (minA.y <= maxB.y && maxA.y >= minB.y) &&
           (minA.z <= maxB.z && maxA.z >= minB.z);
}

// Reinicia uma nova partida: sorteia novas posições aleatórias (distintas)
// para o jogador e para o "smile", gera um novo labirinto a partir da
// célula do jogador, recria os ratinhos e zera o cronômetro. Chamada quando
// o jogador clica/pressiona "Jogar novamente" na tela de fim de jogo.
void ResetGame(GLFWwindow *window)
{
    RandomizeStartAndGoalCells();

    SpawnRats();

    g_GameState = GAME_PLAYING;
    g_GameStartTime = glfwGetTime();
    g_GameOverElapsed = 0.0;
    g_RestartButtonValid = false;

    // Reinicia a trilha de fundo para a nova partida (ela foi parada ao
    // final da partida anterior, junto com a tela de fim de jogo) - mas só
    // se a música estiver habilitada na caixa de seleção do menu inicial.
    if (g_MusicEnabled)
        Audio_PlayBackgroundMusic();

    (void)window;
}

// Inicia a primeira partida a partir da tela de menu (chamada quando o
// jogador clica no botão "Iniciar" em DrawMenuScreen()): sorteia o
// labirinto/posições, espalha os ratos, e começa o cronômetro e a música de
// fundo (esta última apenas se a caixa de seleção de música estiver
// marcada). Equivalente ao que ResetGame() faz para reinícios subsequentes,
// mas mantido como uma função separada para deixar claro, no código do
// menu, que esta é a transição MENU -> PLAYING (e não um reinício de
// partida em andamento).
void StartGameFromMenu()
{
    RandomizeStartAndGoalCells();

    SpawnRats();

    g_GameState = GAME_PLAYING;
    g_GameStartTime = glfwGetTime();
    g_GameOverElapsed = 0.0;
    g_RestartButtonValid = false;

    if (g_MusicEnabled)
        Audio_PlayBackgroundMusic();
}

// ---------------------------------------------------------------------------
// Personagem do jogador: um boneco "explorador" simples (estilo voxel/
// blocky, feito só de cubos coloridos), com chapéu, camisa, calça, braços,
// pernas e uma lanterna na mão. Desenhado com um programa de GPU próprio
// (model+view+projection 3D reais, mas cor sólida por cubo em vez de
// textura), reaproveitando a mesma ideia/estilo do shader de quad 2D acima,
// porém agora operando no espaço 3D do mundo, então o boneco aparece
// corretamente posicionado e ocluso por paredes/objetos da cena.
// ---------------------------------------------------------------------------
static GLuint g_CubeSolidVAO = 0;
static GLuint g_CubeSolidProgramID = 0;
static GLint g_CubeSolidModelUniform = -1;
static GLint g_CubeSolidViewUniform = -1;
static GLint g_CubeSolidProjectionUniform = -1;
static GLint g_CubeSolidColorUniform = -1;
static bool g_CubeSolidInitialized = false;

static const char *g_CubeSolidVertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec4 model_coefficients;\n"
    "layout (location = 1) in vec4 normal_coefficients;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec3 v_normal_worldspace;\n"
    "void main() {\n"
    "    gl_Position = projection * view * model * model_coefficients;\n"
    "    v_normal_worldspace = normalize(mat3(model) * normal_coefficients.xyz);\n"
    "}\n";

static const char *g_CubeSolidFragmentShaderSource =
    "#version 330 core\n"
    "in vec3 v_normal_worldspace;\n"
    "uniform vec4 cubeColor;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.6));\n"
    "    float diff = max(dot(normalize(v_normal_worldspace), lightDir), 0.0);\n"
    "    float ambient = 0.45;\n"
    "    float shade = ambient + (1.0 - ambient) * diff;\n"
    "    color = vec4(cubeColor.rgb * shade, cubeColor.a);\n"
    "}\n";

// Constrói um cubo unitário (lado 1, centrado na origem) com posições e
// normais, usado exclusivamente pelo shader de cor sólida acima.
static void InitCubeSolid()
{
    if (g_CubeSolidInitialized)
        return;

    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    const GLchar *vsrc = g_CubeSolidVertexShaderSource;
    GLint vlen = (GLint)strlen(g_CubeSolidVertexShaderSource);
    glShaderSource(vertex_shader_id, 1, &vsrc, &vlen);
    glCompileShader(vertex_shader_id);

    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *fsrc = g_CubeSolidFragmentShaderSource;
    GLint flen = (GLint)strlen(g_CubeSolidFragmentShaderSource);
    glShaderSource(fragment_shader_id, 1, &fsrc, &flen);
    glCompileShader(fragment_shader_id);

    g_CubeSolidProgramID = glCreateProgram();
    glAttachShader(g_CubeSolidProgramID, vertex_shader_id);
    glAttachShader(g_CubeSolidProgramID, fragment_shader_id);
    glLinkProgram(g_CubeSolidProgramID);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    g_CubeSolidModelUniform = glGetUniformLocation(g_CubeSolidProgramID, "model");
    g_CubeSolidViewUniform = glGetUniformLocation(g_CubeSolidProgramID, "view");
    g_CubeSolidProjectionUniform = glGetUniformLocation(g_CubeSolidProgramID, "projection");
    g_CubeSolidColorUniform = glGetUniformLocation(g_CubeSolidProgramID, "cubeColor");

    float h = 0.5f;
    float vertices[] = {
        // posição (x,y,z,w)      // normal (x,y,z,w)
        // front (+Z)
        -h, -h, h, 1, 0, 0, 1, 0,
        h, -h, h, 1, 0, 0, 1, 0,
        h, h, h, 1, 0, 0, 1, 0,
        h, h, h, 1, 0, 0, 1, 0,
        -h, h, h, 1, 0, 0, 1, 0,
        -h, -h, h, 1, 0, 0, 1, 0,
        // back (-Z)
        h, -h, -h, 1, 0, 0, -1, 0,
        -h, -h, -h, 1, 0, 0, -1, 0,
        -h, h, -h, 1, 0, 0, -1, 0,
        -h, h, -h, 1, 0, 0, -1, 0,
        h, h, -h, 1, 0, 0, -1, 0,
        h, -h, -h, 1, 0, 0, -1, 0,
        // left (-X)
        -h, -h, -h, 1, -1, 0, 0, 0,
        -h, -h, h, 1, -1, 0, 0, 0,
        -h, h, h, 1, -1, 0, 0, 0,
        -h, h, h, 1, -1, 0, 0, 0,
        -h, h, -h, 1, -1, 0, 0, 0,
        -h, -h, -h, 1, -1, 0, 0, 0,
        // right (+X)
        h, -h, h, 1, 1, 0, 0, 0,
        h, -h, -h, 1, 1, 0, 0, 0,
        h, h, -h, 1, 1, 0, 0, 0,
        h, h, -h, 1, 1, 0, 0, 0,
        h, h, h, 1, 1, 0, 0, 0,
        h, -h, h, 1, 1, 0, 0, 0,
        // top (+Y)
        -h, h, h, 1, 0, 1, 0, 0,
        h, h, h, 1, 0, 1, 0, 0,
        h, h, -h, 1, 0, 1, 0, 0,
        h, h, -h, 1, 0, 1, 0, 0,
        -h, h, -h, 1, 0, 1, 0, 0,
        -h, h, h, 1, 0, 1, 0, 0,
        // bottom (-Y)
        -h, -h, -h, 1, 0, -1, 0, 0,
        h, -h, -h, 1, 0, -1, 0, 0,
        h, -h, h, 1, 0, -1, 0, 0,
        h, -h, h, 1, 0, -1, 0, 0,
        -h, -h, h, 1, 0, -1, 0, 0,
        -h, -h, -h, 1, 0, -1, 0, 0,
    };

    GLuint VBO;
    glGenVertexArrays(1, &g_CubeSolidVAO);
    glBindVertexArray(g_CubeSolidVAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    g_CubeSolidInitialized = true;
}

// Desenha um cubo unitário (escalado/posicionado pela matriz "model"
// fornecida) com cor sólida RGBA, usando o shader 3D dedicado acima. As
// matrizes "view" e "projection" devem ser as mesmas usadas para o resto da
// cena, garantindo que o cubo apareça corretamente posicionado/ocluso.
static void DrawSolidCube(const glm::mat4 &model, const glm::mat4 &view,
                           const glm::mat4 &projection, float r, float g, float b, float a)
{
    InitCubeSolid();

    glUseProgram(g_CubeSolidProgramID);
    glUniformMatrix4fv(g_CubeSolidModelUniform, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(g_CubeSolidViewUniform, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_CubeSolidProjectionUniform, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform4f(g_CubeSolidColorUniform, r, g, b, a);

    glBindVertexArray(g_CubeSolidVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glUseProgram(0);
}

// Paleta de cores do boneco explorador (estilo voxel/blocky, sem usar
// nenhuma marca ou personagem licenciado): pele, camisa cáqui, calça
// marrom-escura, chapéu de explorador e uma lanterna cinza/amarela na mão.
static const glm::vec3 kSkinColor = glm::vec3(0.86f, 0.65f, 0.49f);
static const glm::vec3 kShirtColor = glm::vec3(0.42f, 0.50f, 0.27f);
static const glm::vec3 kPantsColor = glm::vec3(0.30f, 0.22f, 0.14f);
static const glm::vec3 kHatColor = glm::vec3(0.55f, 0.42f, 0.20f);
static const glm::vec3 kBootColor = glm::vec3(0.18f, 0.13f, 0.10f);
static const glm::vec3 kLanternBodyColor = glm::vec3(0.25f, 0.25f, 0.27f);
static const glm::vec3 kLanternLightColor = glm::vec3(1.0f, 0.92f, 0.55f);

// Desenha o personagem do jogador (boneco explorador) na posição/orientação
// fornecidas, usando uma hierarquia simples de cubos (torso como "raiz",
// cabeça/braços/pernas pendurados a partir dele). É a representação visual
// da hitbox cúbica do jogador: aparece tanto em primeira pessoa (visto de
// frente, ao colidir com objetos) quanto, mais importante, na visão de cima
// (TAB), onde antes havia apenas um marcador esférico (removido).
void DrawPlayerCharacter(glm::vec3 position, float yaw, const glm::mat4 &view, const glm::mat4 &projection)
{
    // Dimensões em unidades de mundo, calibradas para a hitbox do jogador
    // (cubo de lado 2*g_PlayerHalfWidth) sem ultrapassá-la visualmente.
    float totalHeight = 2.0f * g_PlayerHalfHeight; // pés ao topo da cabeça
    float headSize = totalHeight * 0.22f;
    float torsoHeight = totalHeight * 0.40f;
    float torsoWidth = totalHeight * 0.34f;
    float torsoDepth = totalHeight * 0.20f;
    float legHeight = totalHeight * 0.38f;
    float legWidth = torsoWidth * 0.42f;
    float armHeight = torsoHeight * 0.95f;
    float armWidth = legWidth * 0.85f;
    float hatBrimHeight = headSize * 0.18f;
    float hatTopHeight = headSize * 0.35f;

    float feetY = position.y; // g_PlayerPosition.y já está no chão (g_GroundY)
    float legCenterY = feetY + legHeight * 0.5f;
    float torsoCenterY = feetY + legHeight + torsoHeight * 0.5f;
    float headCenterY = feetY + legHeight + torsoHeight + headSize * 0.5f;

    glm::mat4 baseModel = Matrix_Translate(position.x, 0.0f, position.z) * Matrix_Rotate_Y(yaw);

    auto part = [&](float cx, float cy, float cz, float sx, float sy, float sz, glm::vec3 color)
    {
        glm::mat4 model = baseModel *
                           Matrix_Translate(cx, cy, cz) *
                           Matrix_Scale(sx, sy, sz);
        DrawSolidCube(model, view, projection, color.r, color.g, color.b, 1.0f);
    };

    // Variante de "part" que primeiro roda a peça em torno do eixo X local
    // (pitch), pivotando em um ponto de articulação (pivotY) acima/abaixo do
    // centro da própria peça - usada para o balanço de pernas/braços da
    // animação de andar, simulando uma articulação de quadril/ombro em vez
    // de rotacionar a peça em torno do seu próprio centro geométrico.
    auto swingingPart = [&](float cx, float pivotY, float cz, float sx, float sy, float sz,
                             float lengthBelowPivot, float lengthAbovePivot, float swingAngle, glm::vec3 color)
    {
        // Centro da peça relativo ao pivô: metade para cada lado, conforme
        // quanto da peça fica abaixo/acima do ponto de articulação.
        float centerOffsetY = (lengthAbovePivot - lengthBelowPivot) * 0.5f;

        glm::mat4 model = baseModel *
                           Matrix_Translate(cx, pivotY, cz) *
                           Matrix_Rotate_X(swingAngle) *
                           Matrix_Translate(0.0f, centerOffsetY, 0.0f) *
                           Matrix_Scale(sx, sy, sz);
        DrawSolidCube(model, view, projection, color.r, color.g, color.b, 1.0f);
    };

    // Ângulo de balanço atual de cada perna/braço, derivado da fase de
    // caminhada global (g_PlayerWalkPhase, atualizada no laço principal
    // proporcionalmente à distância percorrida pelo jogador). Pernas e
    // braços do mesmo lado oscilam em fases opostas (perna esquerda para
    // frente quando o braço esquerdo vai para trás, como em uma caminhada
    // normal); o lado direito usa a fase oposta ao esquerdo.
    float leftSwing = sinf(g_PlayerWalkPhase) * g_PlayerWalkSwingAmplitude;
    float rightSwing = -leftSwing;

    // Pernas (levemente separadas no eixo X local), articuladas no quadril
    // (topo da perna, altura "feetY + legHeight").
    float hipY = feetY + legHeight;
    swingingPart(-legWidth * 0.55f, hipY, 0.0f, legWidth, legHeight, legWidth,
                 legHeight, 0.0f, leftSwing, kPantsColor);
    swingingPart(legWidth * 0.55f, hipY, 0.0f, legWidth, legHeight, legWidth,
                 legHeight, 0.0f, rightSwing, kPantsColor);

    // Botas (pequeno cubo escuro na base de cada perna), seguindo o mesmo
    // balanço da perna correspondente (mesmo pivô no quadril, mesmo ângulo),
    // para que fiquem "presas" ao pé em vez de ficarem paradas no chão
    // enquanto a perna se move.
    float bootHeight = legHeight * 0.22f;
    swingingPart(-legWidth * 0.55f, hipY, legWidth * 0.08f, legWidth * 1.05f, bootHeight, legWidth * 1.25f,
                 legHeight - bootHeight * 0.5f, 0.0f, leftSwing, kBootColor);
    swingingPart(legWidth * 0.55f, hipY, legWidth * 0.08f, legWidth * 1.05f, bootHeight, legWidth * 1.25f,
                 legHeight - bootHeight * 0.5f, 0.0f, rightSwing, kBootColor);

    // Torso (camisa).
    part(0.0f, torsoCenterY, 0.0f, torsoWidth, torsoHeight, torsoDepth, kShirtColor);

    // Braços (pele exposta abaixo de uma manga curta, simplificado como um
    // único cubo cor de pele por braço, posicionados nas laterais do torso),
    // articulados no ombro (topo do braço) e balançando em fase oposta à
    // perna do mesmo lado.
    float armOffsetX = torsoWidth * 0.5f + armWidth * 0.5f;
    float shoulderY = torsoCenterY + torsoHeight * 0.05f + armHeight * 0.5f;
    swingingPart(-armOffsetX, shoulderY, 0.0f, armWidth, armHeight, armWidth,
                 armHeight, 0.0f, rightSwing, kSkinColor);
    swingingPart(armOffsetX, shoulderY, 0.0f, armWidth, armHeight, armWidth,
                 armHeight, 0.0f, leftSwing, kSkinColor);

    // Cabeça.
    part(0.0f, headCenterY, 0.0f, headSize, headSize, headSize, kSkinColor);

    // Chapéu de explorador: aba larga e achatada + "copa" mais estreita.
    float hatBrimY = headCenterY + headSize * 0.5f + hatBrimHeight * 0.5f;
    part(0.0f, hatBrimY, 0.0f, headSize * 1.5f, hatBrimHeight, headSize * 1.5f, kHatColor);
    float hatTopY = hatBrimY + hatBrimHeight * 0.5f + hatTopHeight * 0.5f;
    part(0.0f, hatTopY, 0.0f, headSize * 0.85f, hatTopHeight, headSize * 0.85f, kHatColor);

    // Lanterna na mão direita (do ponto de vista do personagem): um pequeno
    // cubo cinza (corpo) com uma face amarelo-claro (luz) na frente, presa
    // à frente do braço direito, um pouco abaixo do ombro, como se o
    // personagem a estivesse segurando esticada para frente. Para manter a
    // lanterna sempre apontando para frente (e não balançando junto com o
    // braço, o que faria a luz "varrer" o chão de forma estranha durante a
    // caminhada), ela é desenhada fora da hierarquia rotacionada do braço,
    // na posição neutra (sem aplicar swingingPart).
    float lanternSize = armWidth * 0.9f;
    float lanternForward = torsoDepth * 0.5f + lanternSize * 0.6f;
    float lanternY = torsoCenterY - torsoHeight * 0.15f;
    part(armOffsetX, lanternY, lanternForward * 0.6f, lanternSize, lanternSize * 1.3f, lanternSize, kLanternBodyColor);
    part(armOffsetX, lanternY, lanternForward * 0.6f + lanternSize * 0.55f, lanternSize * 0.7f, lanternSize * 0.7f, lanternSize * 0.35f, kLanternLightColor);
}


// para o painel de fundo da tela de fim de jogo). Implementado com um
// programa de GPU próprio e bem simples (posição 2D em NDC + cor sólida),
// seguindo o mesmo padrão usado por TextRendering_Init() em textrendering.cpp
// para não interferir no shader principal da cena 3D.
// ---------------------------------------------------------------------------
static GLuint g_QuadVAO = 0;
static GLuint g_QuadVBO = 0;
static GLuint g_QuadProgramID = 0;
static GLint g_QuadColorUniform = -1;
static bool g_QuadInitialized = false;

static const char *g_QuadVertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec2 position;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static const char *g_QuadFragmentShaderSource =
    "#version 330 core\n"
    "uniform vec4 quadColor;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    color = quadColor;\n"
    "}\n";

static void InitColoredQuad()
{
    if (g_QuadInitialized)
        return;

    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    const GLchar *vsrc = g_QuadVertexShaderSource;
    GLint vlen = (GLint)strlen(g_QuadVertexShaderSource);
    glShaderSource(vertex_shader_id, 1, &vsrc, &vlen);
    glCompileShader(vertex_shader_id);

    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *fsrc = g_QuadFragmentShaderSource;
    GLint flen = (GLint)strlen(g_QuadFragmentShaderSource);
    glShaderSource(fragment_shader_id, 1, &fsrc, &flen);
    glCompileShader(fragment_shader_id);

    g_QuadProgramID = glCreateProgram();
    glAttachShader(g_QuadProgramID, vertex_shader_id);
    glAttachShader(g_QuadProgramID, fragment_shader_id);
    glLinkProgram(g_QuadProgramID);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    g_QuadColorUniform = glGetUniformLocation(g_QuadProgramID, "quadColor");

    glGenVertexArrays(1, &g_QuadVAO);
    glGenBuffers(1, &g_QuadVBO);

    glBindVertexArray(g_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    g_QuadInitialized = true;
}

// Desenha um retângulo 2D preenchido, em coordenadas de tela normalizadas
// (NDC: x e y entre -1 e 1), com a cor RGBA informada.
void DrawColoredQuad2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
    InitColoredQuad();

    float vertices[12] = {
        x0, y0,
        x1, y0,
        x1, y1,

        x0, y0,
        x1, y1,
        x0, y1};

    glUseProgram(g_QuadProgramID);
    glUniform4f(g_QuadColorUniform, r, g, b, a);

    glBindVertexArray(g_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_QuadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// Desenha a tela de menu inicial (mostrada antes da primeira partida
// começar): um texto explicando o objetivo do jogo, os controles, uma caixa
// de seleção para ligar/desligar a música de fundo, e o botão "Iniciar".
// Reaproveita o mesmo estilo visual (quads 2D + texto) já usado em
// DrawGameOverScreen(), por consistência.
void DrawMenuScreen(GLFWwindow *window)
{
    // Painel de fundo (branco, semi-transparente) sobre toda a tela.
    DrawColoredQuad2D(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.65f);

    // Painel sólido (opaco, branco) atrás de todo o conteúdo do menu,
    // garantindo legibilidade total do texto independente do que esteja
    // atrás na cena 3D.
    DrawColoredQuad2D(-0.66f, -0.62f, 0.66f, 0.62f, 1.0f, 1.0f, 1.0f, 0.95f);

    std::string title = "LABIRINTO DO RATO";

    // Objetivo do jogo (resumido em poucas linhas curtas, já que o sistema
    // de texto usado não faz quebra automática de linha).
    std::vector<std::string> objectiveLines = {
        "Objetivo:",
        "Explore o labirinto escuro com sua lanterna",
        "e encontre o \"smile\" brilhante para vencer.",
        "Cuidado com os ratinhos pelo caminho!"};

    // Controles do jogo.
    std::vector<std::string> controlLines = {
        "Controles:",
        "WASD - mover     Mouse - olhar",
        "Shift - correr    Espaco - so paredes da borda",
        "TAB - visao de cima    ESC - sair"};

    float titleScale = 2.6f;
    float textScale = 1.1f;
    float lineSpacing = 0.075f;

    float titleX = -0.34f;
    float titleY = 0.52f;
    TextRendering_PrintString(window, title, titleX, titleY, titleScale);

    float y = 0.36f;
    for (const std::string &line : objectiveLines)
    {
        TextRendering_PrintString(window, line, -0.60f, y, textScale);
        y -= lineSpacing;
    }

    y -= 0.04f;
    for (const std::string &line : controlLines)
    {
        TextRendering_PrintString(window, line, -0.60f, y, textScale);
        y -= lineSpacing;
    }

    // Caixa de seleção (checkbox) para ligar/desligar a música de fundo.
    float checkboxSize = 0.05f;
    float checkboxX = -0.60f;
    float checkboxY = -0.22f;

    g_MusicCheckboxMinX = checkboxX;
    g_MusicCheckboxMaxX = checkboxX + checkboxSize;
    g_MusicCheckboxMinY = checkboxY;
    g_MusicCheckboxMaxY = checkboxY + checkboxSize;
    g_MusicCheckboxValid = true;

    // Contorno da caixa (sempre visível) + preenchimento interno (só quando
    // a música está habilitada), simulando um checkbox marcado/desmarcado.
    DrawColoredQuad2D(checkboxX, checkboxY, checkboxX + checkboxSize, checkboxY + checkboxSize, 0.15f, 0.15f, 0.15f, 1.0f);
    if (g_MusicEnabled)
    {
        float innerMargin = checkboxSize * 0.22f;
        DrawColoredQuad2D(
            checkboxX + innerMargin, checkboxY + innerMargin,
            checkboxX + checkboxSize - innerMargin, checkboxY + checkboxSize - innerMargin,
            0.25f, 0.65f, 0.30f, 1.0f);
    }
    else
    {
        // Caixa "vazia" por dentro (branca), deixando claro que está
        // desmarcada mesmo sem depender só da cor do contorno.
        float innerMargin = checkboxSize * 0.12f;
        DrawColoredQuad2D(
            checkboxX + innerMargin, checkboxY + innerMargin,
            checkboxX + checkboxSize - innerMargin, checkboxY + checkboxSize - innerMargin,
            1.0f, 1.0f, 1.0f, 1.0f);
    }

    std::string musicLabel = "Musica do jogo";
    TextRendering_PrintString(window, musicLabel, checkboxX + checkboxSize + 0.03f, checkboxY - 0.005f, textScale);

    // Botão "Iniciar".
    float btnMinX = -0.22f, btnMaxX = 0.22f;
    float btnMinY = -0.52f, btnMaxY = -0.34f;

    g_StartButtonMinX = btnMinX;
    g_StartButtonMaxX = btnMaxX;
    g_StartButtonMinY = btnMinY;
    g_StartButtonMaxY = btnMaxY;
    g_StartButtonValid = true;

    // Detecta hover do mouse sobre o botão e sobre a checkbox, para dar
    // feedback visual simples (mesma técnica usada em DrawGameOverScreen).
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);
    float ndcX = (winW > 0) ? (2.0f * (float)mouseX / (float)winW - 1.0f) : 0.0f;
    float ndcY = (winH > 0) ? (1.0f - 2.0f * (float)mouseY / (float)winH) : 0.0f;
    bool hoveringStart = (ndcX >= btnMinX && ndcX <= btnMaxX && ndcY >= btnMinY && ndcY <= btnMaxY);

    if (hoveringStart)
        DrawColoredQuad2D(btnMinX, btnMinY, btnMaxX, btnMaxY, 0.35f, 0.75f, 0.35f, 1.0f);
    else
        DrawColoredQuad2D(btnMinX, btnMinY, btnMaxX, btnMaxY, 0.20f, 0.55f, 0.20f, 1.0f);

    std::string buttonLabel = "Iniciar";
    float buttonTextScale = 1.8f;
    float buttonTextX = btnMinX + 0.075f;
    float buttonTextY = (btnMinY + btnMaxY) / 2.0f - 0.035f;
    TextRendering_PrintString(window, buttonLabel, buttonTextX, buttonTextY, buttonTextScale);
}

// Desenha a tela de fim de jogo: um painel semitransparente cobrindo a tela,
// o texto "FIM DE JOGO" com o tempo total decorrido, e um botão "Jogar
// novamente" (quad colorido + texto) cuja área é guardada nas variáveis
// globais g_RestartButton* para que MouseButtonCallback() detecte cliques.
void DrawGameOverScreen(GLFWwindow *window, double elapsedSeconds)
{
    // Painel de fundo (branco) sobre toda a tela.
    DrawColoredQuad2D(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.65f);

    // Painel sólido (opaco, branco) atrás do bloco de título+subtítulo,
    // garantindo legibilidade total do texto (preto) independente da
    // textura da parede/teto que esteja por trás na cena 3D.
    DrawColoredQuad2D(-0.62f, 0.0f, 0.62f, 0.34f, 1.0f, 1.0f, 1.0f, 0.92f);

    int totalSeconds = (int)(elapsedSeconds + 0.5);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    char timeBuffer[64];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", minutes, seconds);

    std::string title = "FIM DE JOGO!";
    std::string subtitle = "Voce encontrou o smile em " + std::string(timeBuffer);
    std::string buttonLabel = "Jogar novamente";

    // Título e subtítulo, centralizados aproximadamente pela contagem de
    // caracteres (o sistema de fontes usado não expõe largura total da
    // string de antemão, então usamos uma estimativa simples).
    float titleScale = 3.0f;
    float subtitleScale = 1.6f;

    float titleX = -0.30f;
    float titleY = 0.25f;
    TextRendering_PrintString(window, title, titleX, titleY, titleScale);

    float subtitleX = -0.34f;
    float subtitleY = 0.05f;
    TextRendering_PrintString(window, subtitle, subtitleX, subtitleY, subtitleScale);

    // Botão "Jogar novamente": um quad colorido com o texto centralizado.
    float btnMinX = -0.28f, btnMaxX = 0.28f;
    float btnMinY = -0.30f, btnMaxY = -0.12f;

    g_RestartButtonMinX = btnMinX;
    g_RestartButtonMaxX = btnMaxX;
    g_RestartButtonMinY = btnMinY;
    g_RestartButtonMaxY = btnMaxY;
    g_RestartButtonValid = true;

    // Detecta hover do mouse sobre o botão para dar feedback visual simples.
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);
    float ndcX = (winW > 0) ? (2.0f * (float)mouseX / (float)winW - 1.0f) : 0.0f;
    float ndcY = (winH > 0) ? (1.0f - 2.0f * (float)mouseY / (float)winH) : 0.0f;
    bool hovering = (ndcX >= btnMinX && ndcX <= btnMaxX && ndcY >= btnMinY && ndcY <= btnMaxY);

    if (hovering)
        DrawColoredQuad2D(btnMinX, btnMinY, btnMaxX, btnMaxY, 0.35f, 0.75f, 0.35f, 1.0f);
    else
        DrawColoredQuad2D(btnMinX, btnMinY, btnMaxX, btnMaxY, 0.20f, 0.55f, 0.20f, 1.0f);

    float buttonTextScale = 1.6f;
    float buttonTextX = btnMinX + 0.045f;
    float buttonTextY = (btnMinY + btnMaxY) / 2.0f - 0.03f;
    TextRendering_PrintString(window, buttonLabel, buttonTextX, buttonTextY, buttonTextScale);

    // Dica adicional: tecla R também reinicia o jogo.
    std::string hint = "(ou pressione R)";
    DrawColoredQuad2D(btnMinX - 0.02f, btnMinY - 0.16f, btnMaxX + 0.02f, btnMinY - 0.02f, 1.0f, 1.0f, 1.0f, 0.92f);
    TextRendering_PrintString(window, hint, btnMinX + 0.02f, btnMinY - 0.10f, 1.2f);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char *filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char *filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char *filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try
    {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    }
    catch (std::exception &e)
    {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar *shader_string = str.c_str();
    const GLint shader_string_length = static_cast<GLint>(str.length());

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar *log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if (log_length != 0)
    {
        std::string output;

        if (!compiled_ok)
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete[] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if (linked_ok == GL_FALSE)
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar *log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete[] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    // Tela de menu inicial: um clique do botão esquerdo dentro da área da
    // caixa de seleção liga/desliga a música; um clique dentro do botão
    // "Iniciar" começa a partida.
    if (g_GameState == GAME_MENU && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        int winW, winH;
        glfwGetWindowSize(window, &winW, &winH);
        float ndcX = (winW > 0) ? (2.0f * (float)mouseX / (float)winW - 1.0f) : 0.0f;
        float ndcY = (winH > 0) ? (1.0f - 2.0f * (float)mouseY / (float)winH) : 0.0f;

        if (g_MusicCheckboxValid &&
            ndcX >= g_MusicCheckboxMinX && ndcX <= g_MusicCheckboxMaxX &&
            ndcY >= g_MusicCheckboxMinY && ndcY <= g_MusicCheckboxMaxY)
        {
            g_MusicEnabled = !g_MusicEnabled;
            return;
        }

        if (g_StartButtonValid &&
            ndcX >= g_StartButtonMinX && ndcX <= g_StartButtonMaxX &&
            ndcY >= g_StartButtonMinY && ndcY <= g_StartButtonMaxY)
        {
            StartGameFromMenu();
            if (g_FpsMode)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                g_FirstMouse = true;
            }
            return;
        }
    }

    // Tela de fim de jogo: um clique do botão esquerdo dentro da área do
    // botão "Jogar novamente" reinicia uma nova partida.
    if (g_GameState == GAME_OVER && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if (g_RestartButtonValid)
        {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);
            float ndcX = (winW > 0) ? (2.0f * (float)mouseX / (float)winW - 1.0f) : 0.0f;
            float ndcY = (winH > 0) ? (1.0f - 2.0f * (float)mouseY / (float)winH) : 0.0f;

            if (ndcX >= g_RestartButtonMinX && ndcX <= g_RestartButtonMaxX &&
                ndcY >= g_RestartButtonMinY && ndcY <= g_RestartButtonMaxY)
            {
                ResetGame(window);
                if (g_FpsMode)
                {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    g_FirstMouse = true;
                }
                return;
            }
        }
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_RightMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_MiddleMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    // If FPS mode is enabled, use mouse movement to control yaw/pitch.
    // Durante a tela de menu inicial o cursor fica livre/visível (ver
    // inicialização em main() e MouseButtonCallback()) para que o jogador
    // possa clicar nos botões - por isso não usamos seu movimento para
    // girar a câmera enquanto g_GameState == GAME_MENU.
    if (g_FpsMode && g_GameState != GAME_MENU)
    {
        if (g_FirstMouse)
        {
            g_LastCursorPosX = xpos;
            g_LastCursorPosY = ypos;
            g_FirstMouse = false;
        }

        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        float sensitivity = 0.0025f;
        g_PlayerYaw -= sensitivity * dx;
        g_PlayerPitch -= sensitivity * dy;

        // Clamp pitch
        const float pitchMax = 3.1415926f / 2.0f - 0.01f;
        if (g_PlayerPitch > pitchMax)
            g_PlayerPitch = pitchMax;
        if (g_PlayerPitch < -pitchMax)
            g_PlayerPitch = -pitchMax;

        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
        return;
    }

    if (g_RightMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da antebraço com os deslocamentos
        g_ForearmAngleZ -= 0.01f * dx;
        g_ForearmAngleX += 0.01f * dy;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

    if (g_MiddleMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da antebraço com os deslocamentos
        g_TorsoPositionX += 0.01f * dx;
        g_TorsoPositionY -= 0.01f * dy;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f * yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    if (g_CameraDistance < verysmallnumber)
        g_CameraDistance = verysmallnumber;
}

void Correcao_KeyCallback(int key, int action, int mod);

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mod)
{
    // =======================
    // Não modifique esta chamada! Ela é utilizada para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    Correcao_KeyCallback(key, action, mod);
    // =======================

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        g_TopView = !g_TopView;
    }
    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // Na tela de fim de jogo, a tecla R reinicia uma nova partida (atalho de
    // teclado equivalente a clicar no botão "Jogar novamente").
    if (g_GameState == GAME_OVER && key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        ResetGame(window);
        if (g_FpsMode)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            g_FirstMouse = true;
        }
    }

    // O código abaixo implementa a seguinte lógica:
    //   Se apertar tecla X       então g_AngleX += delta;
    //   Se apertar tecla shift+X então g_AngleX -= delta;
    //   Se apertar tecla Y       então g_AngleY += delta;
    //   Se apertar tecla shift+Y então g_AngleY -= delta;
    //   Se apertar tecla Z       então g_AngleZ += delta;
    //   Se apertar tecla shift+Z então g_AngleZ -= delta;

    float delta = 3.141592 / 16; // 22.5 graus, em radianos.

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        g_AngleX += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        g_AngleY += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        g_AngleZ += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    // Se o usuário apertar a tecla espaço, resetamos os ângulos de Euler para zero.
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_AngleX = 0.0f;
        g_AngleY = 0.0f;
        g_AngleZ = 0.0f;
        g_ForearmAngleX = 0.0f;
        g_ForearmAngleZ = 0.0f;
        g_TorsoPositionX = 0.0f;
        g_TorsoPositionY = 0.0f;
    }

    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout, "Shaders recarregados!\n");
        fflush(stdout);
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_OnlyBorderWalls = !g_OnlyBorderWalls;
    }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char *description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow *window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model)
{
    if (!g_ShowInfoText)
        return;

    glm::vec4 p_world = model * p_model;
    glm::vec4 p_camera = view * p_world;
    glm::vec4 p_clip = projection * p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f - pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f - 2 * pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f - 6 * pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f - 7 * pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f - 8 * pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f - 9 * pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f - 10 * pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f - 14 * pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f - 15 * pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f - 16 * pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f - 17 * pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f - 18 * pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2(0, 0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x) / (b.x - a.x), 0.0f, 0.0f, (b.x * p.x - a.x * q.x) / (b.x - a.x),
        0.0f, (q.y - p.y) / (b.y - a.y), 0.0f, (b.y * p.y - a.y * q.y) / (b.y - a.y),
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f - 22 * pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f - 23 * pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f - 24 * pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f - 25 * pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f - 26 * pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f + pad / 10, -1.0f + 2 * pad / 10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if (g_UsePerspectiveProjection)
        TextRendering_PrintString(window, "Perspective", 1.0f - 13 * charwidth, -1.0f + 2 * lineheight / 10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f - 13 * charwidth, -1.0f + 2 * lineheight / 10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int ellapsed_frames = 0;
    static char buffer[20] = "?? fps";
    static int numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if (ellapsed_seconds > 1.0f)
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f - (numchars + 1) * charwidth, 1.0f - lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel *model)
{
    const tinyobj::attrib_t &attrib = model->attrib;
    const std::vector<tinyobj::shape_t> &shapes = model->shapes;
    const std::vector<tinyobj::material_t> &materials = model->materials;

    printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
    printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
    printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
    printf("# of shapes    : %d\n", (int)shapes.size());
    printf("# of materials : %d\n", (int)materials.size());

    for (size_t v = 0; v < attrib.vertices.size() / 3; v++)
    {
        printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
               static_cast<const double>(attrib.vertices[3 * v + 0]),
               static_cast<const double>(attrib.vertices[3 * v + 1]),
               static_cast<const double>(attrib.vertices[3 * v + 2]));
    }

    for (size_t v = 0; v < attrib.normals.size() / 3; v++)
    {
        printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
               static_cast<const double>(attrib.normals[3 * v + 0]),
               static_cast<const double>(attrib.normals[3 * v + 1]),
               static_cast<const double>(attrib.normals[3 * v + 2]));
    }

    for (size_t v = 0; v < attrib.texcoords.size() / 2; v++)
    {
        printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
               static_cast<const double>(attrib.texcoords[2 * v + 0]),
               static_cast<const double>(attrib.texcoords[2 * v + 1]));
    }

    // For each shape
    for (size_t i = 0; i < shapes.size(); i++)
    {
        printf("shape[%ld].name = %s\n", static_cast<long>(i),
               shapes[i].name.c_str());
        printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
               static_cast<unsigned long>(shapes[i].mesh.indices.size()));

        size_t index_offset = 0;

        assert(shapes[i].mesh.num_face_vertices.size() ==
               shapes[i].mesh.material_ids.size());

        printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
               static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

        // For each face
        for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++)
        {
            size_t fnum = shapes[i].mesh.num_face_vertices[f];

            printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
                   static_cast<unsigned long>(fnum));

            // For each vertex in the face
            for (size_t v = 0; v < fnum; v++)
            {
                tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
                printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
                       static_cast<long>(v), idx.vertex_index, idx.normal_index,
                       idx.texcoord_index);
            }

            printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
                   shapes[i].mesh.material_ids[f]);

            index_offset += fnum;
        }

        printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
               static_cast<unsigned long>(shapes[i].mesh.tags.size()));
        for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++)
        {
            printf("  tag[%ld] = %s ", static_cast<long>(t),
                   shapes[i].mesh.tags[t].name.c_str());
            printf(" ints: [");
            for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j)
            {
                printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
                if (j < (shapes[i].mesh.tags[t].intValues.size() - 1))
                {
                    printf(", ");
                }
            }
            printf("]");

            printf(" floats: [");
            for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j)
            {
                printf("%f", static_cast<const double>(
                                 shapes[i].mesh.tags[t].floatValues[j]));
                if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1))
                {
                    printf(", ");
                }
            }
            printf("]");

            printf(" strings: [");
            for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j)
            {
                printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
                if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1))
                {
                    printf(", ");
                }
            }
            printf("]");
            printf("\n");
        }
    }

    for (size_t i = 0; i < materials.size(); i++)
    {
        printf("material[%ld].name = %s\n", static_cast<long>(i),
               materials[i].name.c_str());
        printf("  material.Ka = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].ambient[0]),
               static_cast<const double>(materials[i].ambient[1]),
               static_cast<const double>(materials[i].ambient[2]));
        printf("  material.Kd = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].diffuse[0]),
               static_cast<const double>(materials[i].diffuse[1]),
               static_cast<const double>(materials[i].diffuse[2]));
        printf("  material.Ks = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].specular[0]),
               static_cast<const double>(materials[i].specular[1]),
               static_cast<const double>(materials[i].specular[2]));
        printf("  material.Tr = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].transmittance[0]),
               static_cast<const double>(materials[i].transmittance[1]),
               static_cast<const double>(materials[i].transmittance[2]));
        printf("  material.Ke = (%f, %f ,%f)\n",
               static_cast<const double>(materials[i].emission[0]),
               static_cast<const double>(materials[i].emission[1]),
               static_cast<const double>(materials[i].emission[2]));
        printf("  material.Ns = %f\n",
               static_cast<const double>(materials[i].shininess));
        printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
        printf("  material.dissolve = %f\n",
               static_cast<const double>(materials[i].dissolve));
        printf("  material.illum = %d\n", materials[i].illum);
        printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
        printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
        printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
        printf("  material.map_Ns = %s\n",
               materials[i].specular_highlight_texname.c_str());
        printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
        printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
        printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
        printf("  <<PBR>>\n");
        printf("  material.Pr     = %f\n", materials[i].roughness);
        printf("  material.Pm     = %f\n", materials[i].metallic);
        printf("  material.Ps     = %f\n", materials[i].sheen);
        printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
        printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
        printf("  material.aniso  = %f\n", materials[i].anisotropy);
        printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
        printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
        printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
        printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
        printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
        printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
        std::map<std::string, std::string>::const_iterator it(
            materials[i].unknown_parameter.begin());
        std::map<std::string, std::string>::const_iterator itEnd(
            materials[i].unknown_parameter.end());

        for (; it != itEnd; it++)
        {
            printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
        }
        printf("\n");
    }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :
