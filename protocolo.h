#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define MAX_JOGADORES 3 // Limite de alunos/jogadores estabelecido

// Estrutura individual de cada jogador
typedef struct {
    int id_jogador;
    int pos_x;
    int pos_y;
    int status; // 1 = Vivo, 0 = Inativo/Bateu
} EstadoJogador;

// O pacote completo (o estado do mundo) que o Servidor envia
typedef struct {
    EstadoJogador jogadores[MAX_JOGADORES];
} EstadoJogo;

// O pacote minúsculo que o Cliente envia
typedef struct {
    char direcao; // 'W', 'A', 'S', 'D'
} ComandoCliente;

#endif
