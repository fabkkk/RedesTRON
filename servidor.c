#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "protocolo.h"

#define PORTA 8080

int main() {
    int server_fd, novo_socket;
    struct sockaddr_in endereco;
    int opt = 1;
    int addrlen = sizeof(endereco);

    // Array para guardar os file descriptors dos sockets dos 3 clientes
    int clientes[MAX_JOGADORES] = {0, 0, 0};
    EstadoJogo estado_atual;

    // Inicializando o estado base das motos
    for(int i = 0; i < MAX_JOGADORES; i++) {
        estado_atual.jogadores[i].id_jogador = i;
        estado_atual.jogadores[i].pos_x = 10 + (i * 10); // Espalha as posições iniciais
        estado_atual.jogadores[i].pos_y = 10;
        estado_atual.jogadores[i].status = 0; // Começam inativos
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Falha no socket"); exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORTA);

    if (bind(server_fd, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Falha no bind"); exit(EXIT_FAILURE);
    }

    // Escuta permitindo até 3 conexões
    if (listen(server_fd, 3) < 0) { 
        perror("Falha no listen"); exit(EXIT_FAILURE);
    }

    printf("Servidor TRON iniciado. Rodando Tick-Rate (200ms)...\n");

    fd_set readfds; // Conjunto de descritores de arquivo para o select

    // LOOP PRINCIPAL DO JOGO (GAME LOOP)
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds); // Adiciona o socket mestre
        int max_sd = server_fd;

        // Adiciona os sockets dos clientes filhos ao conjunto
        for (int i = 0; i < MAX_JOGADORES; i++) {
            int sd = clientes[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Configuração do timeout do select (O Tick-Rate do Jogo)
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200 milissegundos

        // Select observa a rede. Se estourar o timeout, ele prossegue o loop
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0)) {
            printf("Erro na multiplexação\n");
        }

        // EVENTO: Nova conexão chegando
        if (FD_ISSET(server_fd, &readfds)) {
            if ((novo_socket = accept(server_fd, (struct sockaddr *)&endereco, (socklen_t*)&addrlen)) < 0) {
                perror("accept"); exit(EXIT_FAILURE);
            }
            printf("Novo jogador conectado!\n");
            
            // Coloca o novo socket na primeira vaga vazia
            for (int i = 0; i < MAX_JOGADORES; i++) {
                if (clientes[i] == 0) {
                    clientes[i] = novo_socket;
                    estado_atual.jogadores[i].status = 1; // Ativa a moto
                    // Reseta a posição ao conectar
                    estado_atual.jogadores[i].pos_x = 10 + (i * 10);
                    estado_atual.jogadores[i].pos_y = 10;
                    break;
                }
            }
        }

        // EVENTO: Lendo os comandos de movimento dos clientes
        for (int i = 0; i < MAX_JOGADORES; i++) {
            int sd = clientes[i];
            if (FD_ISSET(sd, &readfds)) {
                ComandoCliente comando;
                int valread = recv(sd, &comando, sizeof(ComandoCliente), 0);
                
                if (valread == 0) {
                    // Alguém fechou o jogo
                    printf("Jogador %d desconectou.\n", i);
                    close(sd);
                    clientes[i] = 0;
                    estado_atual.jogadores[i].status = 0;
                } else {
                    // FÍSICA DO JOGO BÁSICA (Avançando a matriz)
                    int novo_x = estado_atual.jogadores[i].pos_x;
                    int novo_y = estado_atual.jogadores[i].pos_y;

                    if(comando.direcao == 'w' || comando.direcao == 'W') novo_y -= 1;
                    if(comando.direcao == 's' || comando.direcao == 'S') novo_y += 1;
                    if(comando.direcao == 'a' || comando.direcao == 'A') novo_x -= 1;
                    if(comando.direcao == 'd' || comando.direcao == 'D') novo_x += 1;

                    // Lógica de Colisão com as Paredes (Limites da Matriz 40x20)
                    // Considerando que as paredes estão nas bordas (x=0, x=39, y=0, y=19)
                    if (novo_x > 0 && novo_x < 39 && novo_y > 0 && novo_y < 19) {
                        estado_atual.jogadores[i].pos_x = novo_x;
                        estado_atual.jogadores[i].pos_y = novo_y;
                    }
                }
            }
        }

        // ATUALIZAÇÃO: Enviando o estado do mapa para todos os sockets conectados
        for (int i = 0; i < MAX_JOGADORES; i++) {
            if (clientes[i] > 0) {
                send(clientes[i], &estado_atual, sizeof(EstadoJogo), 0);
            }
        }
    }
    return 0;
}
