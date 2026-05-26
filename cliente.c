#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#include "protocolo.h"

#define PORTA 8080

// configura o terminal para o modo raw, que desativa o buffer de entrada (assim o sistema le as teclas imediatamente sem precisar do enter) e desativa o eco (nao exibe na tela as letras digitadas)
void configurar_terminal_raw() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO); // desliga o buffer e o eco das teclas digitadas
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // define a leitura do teclado como nao-bloqueante (assincrona) para evitar que o programa congele esperando digitação
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char ip_servidor[100] = "127.0.0.1";

    printf("Conectando ao servidor %s:%d...\n", ip_servidor, PORTA);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Erro na criacao do socket \n"); return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA);

    if (inet_pton(AF_INET, ip_servidor, &serv_addr.sin_addr) <= 0) {
        printf("\nEndereco IP invalido ou formato incorreto.\n");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nFalha na conexao. O servidor esta rodando?\n");
        close(sock);
        return -1;
    }

    // recebe via socket o id numerico exclusivo atribuido a este jogador pelo servidor
    int meu_id = -1;
    if (recv(sock, &meu_id, sizeof(int), 0) <= 0) {
        printf("\nErro ao obter ID do jogador local do servidor.\n");
        close(sock);
        return -1;
    }

    printf("\nConectado com sucesso!\n");
    printf("Preparando arena... Movimente-se usando WASD. Pressione Q para sair.\n");
    sleep(1);

    configurar_terminal_raw();

    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds); // zera o conjunto de descritores que serao monitorados pela rede e teclado
        FD_SET(sock, &readfds); // adiciona o socket de rede da conexao no conjunto de monitoramento
        FD_SET(STDIN_FILENO, &readfds); // adiciona a entrada padrao do teclado (stdin) no conjunto de monitoramento

        // multiplexacao usando a funcao select, que vigia a placa de rede e o teclado ao mesmo tempo e destrava o programa assim que houver atividade em qualquer um deles
        select(sock + 1, &readfds, NULL, NULL, NULL);

        // captura a entrada do teclado caso o usuario tenha pressionado alguma tecla
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == 'q' || c == 'Q') break; // encerra a conexao e sai da partida se a tecla for q

                // envia comandos de movimento ou reinicio validos para a rede
                if (c == 'w' || c == 'W' || c == 'a' || c == 'A' ||
                    c == 's' || c == 'S' || c == 'd' || c == 'D' ||
                    c == 'r' || c == 'R') {
                    ComandoCliente comando;
                    comando.direcao = c;
                    send(sock, &comando, sizeof(ComandoCliente), 0);
                }
            }
        }

        // escuta dados recebidos do servidor via socket de rede e redesenha a arena na tela
        if (FD_ISSET(sock, &readfds)) {
            EstadoJogo estado;
            int bytes = recv(sock, &estado, sizeof(EstadoJogo), 0);
            if (bytes <= 0) {
                printf("\nO Servidor encerrou a conexao.\n");
                break;
            }

            // limpa a tela do terminal enviando codigos de escape ansi para redesenhar o proximo frame
            printf("\033[H\033[J");

            // informacoes da partida
            printf("--- ARENA TRON --- [Conectados: %d | Vivos: %d]\n", estado.num_jogadores_conectados, estado.num_jogadores_vivos);

            // status da partida
            if (estado.estado_partida == 0) {
                if (estado.contagem_regressiva > 0) printf(" Status: INICIANDO EM %d SEGUNDOS...\n", estado.contagem_regressiva);
                else printf(" Status: AGUARDANDO JOGADORES (Minimo 2 para iniciar)...\n");
            } 
            else if (estado.estado_partida == 1) {
                if (estado.jogadores[meu_id].status == 0) printf(" Status: VOCE FOI ELIMINADO! Aguarde a rodada acabar.\n");
                else printf(" Status: EM JOGO! (Controles: W, A, S, D | Sair: Q)\n");
            } 
            else if (estado.estado_partida == 2) {
                if (estado.vencedor_id != -1) printf(" Status: VENCEDOR MOTO %d! (Aperte R para recomecar ou Q para sair)\n", estado.vencedor_id + 1);
                else printf(" Status: EMPATE GERAL! (Aperte R para recomecar ou Q para sair)\n");
            }

            // renderiza a arena de jogo bidimensional de tamanho 40 colunas por 20 linhas
            for (int y = 0; y < 20; y++) {
                for (int x = 0; x < 40; x++) {
                    int desenhou_algo = 0;

                    for (int i = 0; i < MAX_JOGADORES; i++) {
                        if (estado.jogadores[i].status == 1 && estado.jogadores[i].pos_x == x && estado.jogadores[i].pos_y == y) {
                            printf("%d", i + 1);
                            desenhou_algo = 1;
                            break;
                        }
                    }

                    if (!desenhou_algo) {
                        int rastro = estado.arena[y][x];
                        if (rastro != 0) {
                            printf("o");
                            desenhou_algo = 1;
                        }
                    }

                    if (!desenhou_algo) {
                        if (y == 0 || y == 19 || x == 0 || x == 39) printf("#");
                        else printf(" ");
                    }
                }
                printf("\n"); 
            }
            
            // Força o sistema operacional a imprimir tudo instantaneamente na tela sem esperar o buffer
            fflush(stdout); 
        }
    }

    // desativa o modo raw e devolve as configuracoes normais de buffer de teclado e eco de caracteres ao terminal do sistema operacional
    close(sock);
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

    printf("\nVoce saiu da partida. Ate logo!\n");
    return 0;
}
