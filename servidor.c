#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "protocolo.h"

#define PORTA 8080

// apaga o rastro do jogador na arena
void limpar_rastro_jogador(EstadoJogo *estado, int id_jogador) {
    for (int y = 0; y < 20; y++) { //controle para percorrer as linhas da matriz 
        for (int x = 0; x < 40; x++) {
            if (estado->arena[y][x] == id_jogador + 1) { // se o valor da matriz for igual ao id do jogador + 1, significa que ali existe um rastro deixado pelo jogador (0 = vazio)
                estado->arena[y][x] = 0;
            }
        }
    }
}

// reseta tudo e coloca os jogadores para começar uma nova rodada
void inicializar_rodada(EstadoJogo *estado, int clientes[]) {
    memset(estado->arena, 0, sizeof(estado->arena)); //apaga o mapa inteiro, zerando todas as células

    // coordenadas no mapa pra que as motos comecem separadas
    int pos_iniciais_x[MAX_JOGADORES] = {10, 30, 20};
    int pos_iniciais_y[MAX_JOGADORES] = {5, 15, 10};

    int ativos = 0;
    for (int i = 0; i < MAX_JOGADORES; i++) {
        estado->jogadores[i].id_jogador = i;
        estado->jogadores[i].pos_x = pos_iniciais_x[i];
        estado->jogadores[i].pos_y = pos_iniciais_y[i];

        if (clientes[i] > 0) {
            estado->jogadores[i].status = 1; // se existir uma conexao de cliente valida nesta vaga, o jogador inicia ativo e vivo na partida
            ativos++;
        } else {
            estado->jogadores[i].status = 0; // se a vaga estiver vazia, o jogador comeca inativo e sem moto na arena
        }
    }

    estado->num_jogadores_vivos = ativos;
    estado->vencedor_id = -1;
    estado->contagem_regressiva = 0;
    printf("[SERVIDOR] Rodada inicializada! Jogadores vivos: %d\n", ativos);
}

int main() {
    int server_fd, novo_socket;
    struct sockaddr_in endereco;
    int opt = 1;
    int addrlen = sizeof(endereco);

    // vetor (array) que armazena os identificadores de arquivos (file descriptors) dos sockets dos clientes conectados
    int clientes[MAX_JOGADORES] = {0, 0, 0};
    EstadoJogo estado_atual;

    // limpa toda a estrutura de estado do jogo com zeros usando a funcao memset antes do inicio oficial
    memset(&estado_atual, 0, sizeof(EstadoJogo));
    estado_atual.estado_partida = 0; // define o estado inicial do jogo como aguardando novos jogadores de rede
    estado_atual.vencedor_id = -1;
    estado_atual.contagem_regressiva = 0;

    for (int i = 0; i < MAX_JOGADORES; i++) {
        estado_atual.jogadores[i].id_jogador = i;
        estado_atual.jogadores[i].status = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Falha no socket"); exit(EXIT_FAILURE);
    }

    // configura as propriedades do socket de rede para permitir que a porta 8080 seja liberada e reutilizada imediatamente apos o servidor fechar, evitando erros de porta ocupada
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORTA);

    if (bind(server_fd, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Falha no bind"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Falha no listen"); exit(EXIT_FAILURE);
    }

    printf("Servidor TRON iniciado na porta %d. Rodando Tick-Rate (200ms)...\n", PORTA);

    fd_set readfds;
    int ticks_contagem = 0; // variavel usada para contar os ticks de clock de rede do jogo e controlar temporizadores (como a contagem regressiva)

    // laco infinito do jogo (game loop) onde o processamento ocorre continuadamente a cada intervalo de tick rate
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_JOGADORES; i++) {
            int sd = clientes[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // define o timeout do select para 200 milissegundos, agindo como o ciclo de clock (tick rate) que atualiza o estado do jogo e envia dados em tempo real

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            printf("Erro na multiplexacao\n");
        }

        // detecta atividade no descritor do servidor principal, indicando que um novo jogador esta tentando se conectar
        if (FD_ISSET(server_fd, &readfds)) {
            if ((novo_socket = accept(server_fd, (struct sockaddr *)&endereco, (socklen_t*)&addrlen)) < 0) {
                perror("accept"); exit(EXIT_FAILURE);
            }
            
            // percorre o vetor de clientes procurando o primeiro slot vazio (com valor zero) para registrar o descritor de conexao deste novo jogador
            int index_conectado = -1;
            for (int i = 0; i < MAX_JOGADORES; i++) {
                if (clientes[i] == 0) {
                    clientes[i] = novo_socket;
                    index_conectado = i;
                    break;
                }
            }

            if (index_conectado != -1) {
                printf("[SERVIDOR] Jogador %d conectado!\n", index_conectado + 1);
                
                // envia via socket de rede um pacote inicial contendo apenas o numero do id unico deste cliente para que ele saiba qual moto ele controla
                send(novo_socket, &index_conectado, sizeof(int), 0);
                
                // se a partida estiver na tela de espera (estado 0), o novo jogador entra ativo mas fica congelado ate comecar a rodada, senao ele entra apenas como espectador
                if (estado_atual.estado_partida == 0) {
                    // posicoes e ativa temporariamente
                    estado_atual.jogadores[index_conectado].id_jogador = index_conectado;
                    estado_atual.jogadores[index_conectado].status = 0; // inativo ate rodada iniciar no servidor
                } else {
                    // entra como espectador na rodada que ja esta ocorrendo
                    estado_atual.jogadores[index_conectado].id_jogador = index_conectado;
                    estado_atual.jogadores[index_conectado].status = 0;
                    estado_atual.jogadores[index_conectado].pos_x = 0;
                    estado_atual.jogadores[index_conectado].pos_y = 0;
                }
            } else {
                printf("[SERVIDOR] Servidor cheio! Conexao recusada.\n");
                close(novo_socket);
            }
        }

        // varre o vetor de descritores de sockets dos clientes conectados para verificar se algum deles enviou uma acao pelo teclado
        for (int i = 0; i < MAX_JOGADORES; i++) {
            int sd = clientes[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                ComandoCliente comando;
                int valread = recv(sd, &comando, sizeof(ComandoCliente), 0);

                if (valread == 0) {
                    // se a funcao de leitura (recv) retornar 0 ou menor, significa que a conexao de rede com o cliente foi cortada
                    printf("[SERVIDOR] Jogador %d desconectou.\n", i + 1);
                    close(sd);
                    clientes[i] = 0;

                    // se o jogador que acabou de cair da rede estava ativo, limpa imediatamente todo o rastro dele da arena para liberar espaço
                    if (estado_atual.jogadores[i].status == 1) {
                        limpar_rastro_jogador(&estado_atual, i);
                    }
                    estado_atual.jogadores[i].status = 0;
                } else {
                    // escuta o comando de tecla r do cliente e reinicia a arena de jogo imediatamente se a rodada atual estiver finalizada e houver pessoas suficientes
                    if (comando.direcao == 'r' || comando.direcao == 'R') {
                        if (estado_atual.estado_partida == 2 && conectados >= 2) {
                            printf("[SERVIDOR] Jogador %d solicitou reiniciar a partida.\n", i + 1);
                            estado_atual.estado_partida = 1;
                            inicializar_rodada(&estado_atual, clientes);
                        }
                    }

                    // processa comandos de movimento (w, a, s, d) apenas se a partida estiver rodando e a moto do jogador estiver ativa
                    if (estado_atual.estado_partida == 1 && estado_atual.jogadores[i].status == 1) {
                        int novo_x = estado_atual.jogadores[i].pos_x;
                        int novo_y = estado_atual.jogadores[i].pos_y;

                        if (comando.direcao == 'w' || comando.direcao == 'W') novo_y -= 1;
                        else if (comando.direcao == 's' || comando.direcao == 'S') novo_y += 1;
                        else if (comando.direcao == 'a' || comando.direcao == 'A') novo_x -= 1;
                        else if (comando.direcao == 'd' || comando.direcao == 'D') novo_x += 1;

                        // se mudou de posicao em relacao ao ultimo tick
                        if (novo_x != estado_atual.jogadores[i].pos_x || novo_y != estado_atual.jogadores[i].pos_y) {
                            // grava o numero identificador deste jogador na posicao atual da matriz antes de move-lo, criando o rastro que outras motos nao podem colidir
                            estado_atual.arena[estado_atual.jogadores[i].pos_y][estado_atual.jogadores[i].pos_x] = i + 1;

                            // verifica se a nova coordenada calculada representa uma batida invalida na parede ou em algum rastro (colisao)
                            int colidiu = 0;
                            // detecta se o jogador ultrapassou os limites fisicos das bordas externas da matriz de 40 colunas por 20 linhas (bateu na parede)
                            if (novo_x <= 0 || novo_x >= 39 || novo_y <= 0 || novo_y >= 19) {
                                colidiu = 1;
                            }
                            // detecta se a moto tentou entrar em uma coordenada cujo valor da matriz da arena e diferente de zero (bateu no rastro)
                            else if (estado_atual.arena[novo_y][novo_x] != 0) {
                                colidiu = 1;
                            }

                            if (colidiu) {
                                printf("[SERVIDOR] Jogador %d colidiu e foi eliminado!\n", i + 1);
                                estado_atual.jogadores[i].status = 0; // muda o status do jogador para inativo/eliminado
                                // apaga imediatamente todo o rastro que este jogador derrotado tinha deixado na matriz
                                limpar_rastro_jogador(&estado_atual, i);
                            } else {
                                // atualiza posicao com sucesso caso nao tenha ocorrido batida
                                estado_atual.jogadores[i].pos_x = novo_x;
                                estado_atual.jogadores[i].pos_y = novo_y;
                            }
                        }
                    }
                }
            }
        }

        // bloco de controle de transicao de estados (maquina de estados) que calcula dados de jogadores conectados e vivos
        int conectados = 0;
        int vivos = 0;
        for (int i = 0; i < MAX_JOGADORES; i++) {
            if (clientes[i] > 0) {
                conectados++;
                if (estado_atual.jogadores[i].status == 1) {
                    vivos++;
                }
            }
        }
        estado_atual.num_jogadores_conectados = conectados;
        estado_atual.num_jogadores_vivos = vivos;

        // estado 0: gerencia o estado inicial de espera, iniciando a contagem de 5 segundos se houver pelo menos dois competidores conectados
        if (estado_atual.estado_partida == 0) {
            if (conectados >= 2) {
                if (ticks_contagem == 0) {
                    ticks_contagem = 25; // tempo total de 5 segundos (25 ticks de clock de 200ms)
                }
                ticks_contagem--;
                // calcula segundos restantes para mostrar na interface
                estado_atual.contagem_regressiva = (ticks_contagem * 200 + 999) / 1000;

                if (ticks_contagem <= 0) {
                    ticks_contagem = 0;
                    estado_atual.estado_partida = 1; // comeca partida oficialmente
                    inicializar_rodada(&estado_atual, clientes);
                }
            } else {
                ticks_contagem = 0;
                estado_atual.contagem_regressiva = 0;
            }
        }
        // estado 1: gerencia a partida em andamento atualizando os movimentos e checando condicoes de vitoria
        else if (estado_atual.estado_partida == 1) {
            if (conectados < 2) {
                // se sobrou menos de 2 jogadores ativos apos quedas de conexao, cancela a rodada e retorna a tela de aguardando
                printf("[SERVIDOR] Jogadores insuficientes conectados. Retornando ao estado de espera.\n");
                estado_atual.estado_partida = 0;
                ticks_contagem = 0;
                estado_atual.contagem_regressiva = 0;
                // desativa todos os jogadores da partida atual
                for(int i = 0; i < MAX_JOGADORES; i++) {
                    estado_atual.jogadores[i].status = 0;
                }
            } else {
                // monitora fim de rodada checando se restou apenas um sobrevivente
                if (vivos == 1) {
                    // busca o unico jogador que sobrou vivo para declara-lo vencedor
                    int vencedor = -1;
                    for (int i = 0; i < MAX_JOGADORES; i++) {
                        if (clientes[i] > 0 && estado_atual.jogadores[i].status == 1) {
                            vencedor = i;
                            break;
                        }
                    }
                    printf("[SERVIDOR] Fim de rodada! Vencedor: Jogador %d\n", vencedor + 1);
                    estado_atual.estado_partida = 2; // fim de rodada finalizada
                    estado_atual.vencedor_id = vencedor;
                    ticks_contagem = 0;
                    estado_atual.contagem_regressiva = 0;
                } 
                else if (vivos == 0) {
                    // detecta se as ultimas motos ativas colidiram exatamente no mesmo tick de tempo de 200ms, gerando um empate geral
                    printf("[SERVIDOR] Fim de rodada! Empate geral (todos os jogadores foram eliminados).\n");
                    estado_atual.estado_partida = 2;
                    estado_atual.vencedor_id = -1;
                    ticks_contagem = 0;
                    estado_atual.contagem_regressiva = 0;
                }
            }
        }
        // estado 2: gerencia a tela final apos um jogador vencer ou ocorrer empate geral, aguardando que alguem aperte r para recomecar
        else if (estado_atual.estado_partida == 2) {
            if (conectados < 2) {
                printf("[SERVIDOR] Jogadores insuficientes para reiniciar. Retornando ao estado de espera.\n");
                estado_atual.estado_partida = 0;
                estado_atual.contagem_regressiva = 0;
            }
        }

        // transmite via conexao socket tcp o pacote completo contendo todo o estado atualizado do mundo para cada um dos jogadores ativos na partida
        for (int i = 0; i < MAX_JOGADORES; i++) {
            if (clientes[i] > 0) {
                send(clientes[i], &estado_atual, sizeof(EstadoJogo), 0);
            }
        }
    }

    return 0;
}
