#ifndef AUDIO_H
#define AUDIO_H

// ===========================================================================
// Sistema de áudio do jogo, implementado sobre a biblioteca miniaudio
// (single-header, licença pública/MIT-0 - veja include/miniaudio.h).
//
// Responsabilidades:
//  - Tocar a trilha de fundo (RPG) em loop contínuo até o fim da partida.
//  - Tocar um som de vitória ao encostar no "smile".
//  - Tocar um som de guincho (um dentre vários, escolhido aleatoriamente)
//    quando algum rato estiver visível para o jogador.
//  - Tocar o som de "rato assustado" no momento da colisão jogador-rato.
// ===========================================================================

// Inicializa o engine de áudio e carrega todos os sons usados pelo jogo.
// Deve ser chamada uma única vez, no início de main(), antes de qualquer
// outra função Audio_*. Em caso de falha (ex.: nenhum dispositivo de áudio
// disponível no sistema), o jogo continua funcionando normalmente, apenas
// sem som - todas as funções abaixo verificam internamente se o engine foi
// inicializado com sucesso antes de tentar tocar qualquer som.
void Audio_Init();

// Libera os recursos do engine de áudio. Chamar ao final de main(), antes
// de encerrar o programa.
void Audio_Shutdown();

// Inicia a trilha de fundo (música de exploração estilo RPG) em loop
// contínuo. Chamada uma vez ao iniciar uma partida (em main() e em
// ResetGame()). Se a música já estiver tocando, não faz nada (evita
// reiniciar a música do zero a cada reinício de partida).
void Audio_PlayBackgroundMusic();

// Para a trilha de fundo. Chamada quando o jogo termina (estado GAME_OVER),
// já que a música deve tocar "em loop até o jogo acabar".
void Audio_StopBackgroundMusic();

// Toca o som de vitória (uma única vez, do início ao fim, sem loop). Chamada
// no exato momento em que o jogador encosta no "smile".
void Audio_PlayWinSound();

// Toca o som de "rato assustado" (uma única vez). Chamada no momento em que
// o jogador colide com um rato (o mesmo instante em que o rato passa a
// fugir).
void Audio_PlayRatScaredSound();

// Deve ser chamada uma vez por frame, em main(), com o "dt" do frame atual e
// se há (ou não) pelo menos um rato visível para o jogador neste frame.
// Toca, aleatoriamente e com um intervalo mínimo entre repetições (para não
// soar repetitivo/poluído), um som de guincho de rato sempre que
// "anyRatVisible" for verdadeiro.
void Audio_UpdateRatSqueaks(float dt, bool anyRatVisible);

#endif
