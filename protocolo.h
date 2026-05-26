#ifndef PROTOCOLO_H
#define PROTOCOLO_H

// define o limite maximo de competidores que podem jogar na arena ao mesmo tempo
#define MAX_JOGADORES 3 

// estrutura individual de cada jogador que guarda suas informacoes de identificacao, coordenadas na matriz e se ele ainda esta ativo ou colidiu
typedef struct {
    int id_jogador;
    int pos_x;
    int pos_y;
    int status; // indica se o jogador esta vivo (1) ou se ele colidiu e esta fora da rodada (0)
} EstadoJogador;

// pacote de dados completo contendo o estado do mundo do jogo que o servidor transmite para todos os clientes em cada ciclo
typedef struct {
    EstadoJogador jogadores[MAX_JOGADORES];
    char arena[20][40]; // matriz da arena de jogo, onde 0 significa espaco vazio e qualquer outro valor acima de 0 representa o rastro deixado por uma moto de id (valor - 1)
    int estado_partida; // controla o fluxo da partida (0 para aguardando jogadores, 1 para partida rodando e 2 para rodada finalizada)
    int vencedor_id;    // id numerico da moto vencedora da rodada ou -1 se todos colidirem e houver um empate geral
    int contagem_regressiva; // segundos restantes que aparecem na tela para o inicio da partida
    int num_jogadores_conectados;
    int num_jogadores_vivos;
} EstadoJogo;

// pacote de dados pequeno enviado pelo cliente para o servidor comunicando a acao do jogador
typedef struct {
    char direcao; // caractere que indica a direcao escolhida (w, a, s, d) ou o comando de reiniciar (r)
} ComandoCliente;

#endif
