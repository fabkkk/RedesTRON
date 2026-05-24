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

// Altera o terminal do Linux para ler syscalls do teclado sem precisar de 'Enter'
void configurar_terminal_raw() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO); // Desliga buffer e o eco na tela
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // Torna leitura assíncrona
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Erro no socket \n"); return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nEndereço IP inválido\n"); return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nFalha na Conexão. O servidor está rodando?\n"); return -1;
    }

    printf("Conectado! Pressione W, A, S, D para mover. Pressione Q para sair.\n");
    configurar_terminal_raw();

    fd_set readfds;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds); // Escuta a rede TCP
        FD_SET(STDIN_FILENO, &readfds); // Escuta o teclado (Syscall 0)

        // Select monitora tanto a placa de rede quanto o hardware do teclado
        select(sock + 1, &readfds, NULL, NULL, NULL);

        // 1. CAPTURANDO O TECLADO
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == 'q' || c == 'Q') break; // Sai do jogo
                
                ComandoCliente comando;
                comando.direcao = c;
                send(sock, &comando, sizeof(ComandoCliente), 0);
            }
        }

        // 2. RECEBENDO DADOS DA REDE E RENDERIZANDO A TELA
        if (FD_ISSET(sock, &readfds)) {
            EstadoJogo estado;
            int bytes = recv(sock, &estado, sizeof(EstadoJogo), 0);
            if (bytes <= 0) {
                printf("\nO Servidor fechou a conexão.\n");
                break;
            }

            // LIMPA A TELA PARA O PRÓXIMO FRAME
            printf("\033[H\033[J"); 
            printf("--- ARENA TRON ---\n");

            // RENDERIZA A MATRIZ (20 de altura x 40 de largura)
            for (int y = 0; y < 20; y++) {
                for (int x = 0; x < 40; x++) {
                    int desenhou_algo = 0;

                    // Verifica se há alguma moto nesta coordenada X,Y
                    for(int i = 0; i < MAX_JOGADORES; i++) {
                        if(estado.jogadores[i].status == 1 && estado.jogadores[i].pos_x == x && estado.jogadores[i].pos_y == y) {
                            printf("%d", i + 1); // Imprime o ID da moto (1, 2 ou 3)
                            desenhou_algo = 1;
                            break;
                        }
                    }

                    // Se não tiver moto, desenha a parede ou o chão vazio
                    if (!desenhou_algo) {
                        if (y == 0 || y == 19 || x == 0 || x == 39) {
                            printf("#"); // Bordas (Paredes)
                        } else {
                            printf(" "); // Chão vazio
                        }
                    }
                }
                printf("\n"); // Quebra de linha ao final de cada eixo X
            }
        }
    }

    // Encerrando a aplicação e restaurando o terminal padrão
    close(sock);
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

    return 0;
}
