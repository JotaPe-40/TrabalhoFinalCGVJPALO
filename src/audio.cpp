#include "audio.h"

#include <random>
#include <string>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#define MA_NO_DECODING_BACKEND_LIBOPUS
#include "miniaudio.h"

// ===========================================================================
// Caminhos dos arquivos de áudio (relativos à raiz do projeto, resolvidos
// em tempo de execução por ResolveAssetPath() abaixo - mesma convenção de
// prefixos já usada por FindFile() em main.cpp para os arquivos de imagem
// em assets/). Diferente de FindFile(), aqui a falha em localizar o
// arquivo NÃO encerra o programa: o jogo simplesmente continua sem aquele
// som específico, já que áudio é um recurso não essencial para jogar.
// ===========================================================================
static const char *kBackgroundMusicPath = "assets/sound/rubyzephyr-fantasy-rpg-exploration-v2-461303.mp3";
static const char *kWinSoundPath = "assets/sound/puyopuyomegafan1234-winner-game-sound-404167.mp3";
static const char *kRatScaredSoundPath = "assets/sound/sound_garage-rat-squeaks-scared-fx-396297.mp3";
static const char *kRatSqueakPaths[] = {
    "assets/sound/sound_garage-rat-squeaks-2-fx-396301.mp3",
    "assets/sound/sound_garage-rat-squeaks-3-fx-396299.mp3",
    "assets/sound/sound_garage-rat-squeaks-4-fx-396298.mp3",
};
static const int kNumRatSqueaks = sizeof(kRatSqueakPaths) / sizeof(kRatSqueakPaths[0]);

// O jogo é executado a partir de bin/Linux/ (ou bin/Debug|Release/ no
// Windows), então os caminhos relativos acima precisam subir um ou dois
// níveis de diretório para alcançar a pasta assets/ na raiz do projeto.
// Usa os mesmos prefixos testados por FindFile() em main.cpp, mas sem
// encerrar o programa caso o arquivo não seja encontrado em nenhum deles.
static std::string ResolveAssetPath(const std::string &path)
{
    std::vector<std::string> prefixes = {"", "../", "../../"};

    for (const std::string &prefix : prefixes)
    {
        std::string fullpath = prefix + path;
        std::ifstream file(fullpath);
        if (file.good())
            return fullpath;
    }

    return path;
}

static ma_engine g_AudioEngine;
static bool g_AudioEngineReady = false;

static ma_sound g_BackgroundMusicSound;
static bool g_BackgroundMusicLoaded = false;
static bool g_BackgroundMusicPlaying = false;

static std::mt19937 g_AudioRng(2026u);

// Controla o intervalo mínimo entre guinchos de rato consecutivos, para o
// som não ficar repetitivo/poluído mesmo com vários ratos visíveis ao
// mesmo tempo por um período prolongado.
static float g_RatSqueakCooldown = 0.0f;
static const float kRatSqueakMinInterval = 1.6f;
static const float kRatSqueakMaxInterval = 4.0f;

void Audio_Init()
{
    ma_result result = ma_engine_init(NULL, &g_AudioEngine);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "AVISO: falha ao inicializar o engine de audio (miniaudio). O jogo vai continuar sem som.\n");
        g_AudioEngineReady = false;
        return;
    }

    g_AudioEngineReady = true;

    std::string musicPath = ResolveAssetPath(kBackgroundMusicPath);
    result = ma_sound_init_from_file(&g_AudioEngine, musicPath.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &g_BackgroundMusicSound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "AVISO: falha ao carregar a trilha de fundo (%s).\n", musicPath.c_str());
        g_BackgroundMusicLoaded = false;
    }
    else
    {
        ma_sound_set_looping(&g_BackgroundMusicSound, MA_TRUE);
        g_BackgroundMusicLoaded = true;
    }
}

void Audio_Shutdown()
{
    if (!g_AudioEngineReady)
        return;

    if (g_BackgroundMusicLoaded)
    {
        ma_sound_stop(&g_BackgroundMusicSound);
        ma_sound_uninit(&g_BackgroundMusicSound);
        g_BackgroundMusicLoaded = false;
    }

    ma_engine_uninit(&g_AudioEngine);
    g_AudioEngineReady = false;
}

void Audio_PlayBackgroundMusic()
{
    if (!g_AudioEngineReady || !g_BackgroundMusicLoaded)
        return;

    if (g_BackgroundMusicPlaying)
        return;

    ma_sound_seek_to_pcm_frame(&g_BackgroundMusicSound, 0);
    ma_sound_start(&g_BackgroundMusicSound);
    g_BackgroundMusicPlaying = true;
}

void Audio_StopBackgroundMusic()
{
    if (!g_AudioEngineReady || !g_BackgroundMusicLoaded)
        return;

    ma_sound_stop(&g_BackgroundMusicSound);
    g_BackgroundMusicPlaying = false;
}

void Audio_PlayWinSound()
{
    if (!g_AudioEngineReady)
        return;

    std::string path = ResolveAssetPath(kWinSoundPath);
    ma_engine_play_sound(&g_AudioEngine, path.c_str(), NULL);
}

void Audio_PlayRatScaredSound()
{
    if (!g_AudioEngineReady)
        return;

    std::string path = ResolveAssetPath(kRatScaredSoundPath);
    ma_engine_play_sound(&g_AudioEngine, path.c_str(), NULL);
}

void Audio_UpdateRatSqueaks(float dt, bool anyRatVisible)
{
    if (!g_AudioEngineReady)
        return;

    if (g_RatSqueakCooldown > 0.0f)
        g_RatSqueakCooldown -= dt;

    if (!anyRatVisible)
        return;

    if (g_RatSqueakCooldown > 0.0f)
        return;

    std::uniform_int_distribution<int> soundDist(0, kNumRatSqueaks - 1);
    int chosen = soundDist(g_AudioRng);

    std::string path = ResolveAssetPath(kRatSqueakPaths[chosen]);
    ma_engine_play_sound(&g_AudioEngine, path.c_str(), NULL);

    std::uniform_real_distribution<float> intervalDist(kRatSqueakMinInterval, kRatSqueakMaxInterval);
    g_RatSqueakCooldown = intervalDist(g_AudioRng);
}
