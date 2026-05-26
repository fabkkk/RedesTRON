# RedesTRON

Este repositório contém o código-fonte para o **redesTRON**, um jogo multiplayer em tempo real desenvolvido sobre sockets de rede usando a linguagem C (POSIX). O projeto é o primeiro trabalho prático avaliativo da disciplina Redes de Computadores I.

O jogo é inspirado no clássico jogo de fliperama do filme *Tron*, no qual os jogadores controlam motos de luz em uma arena fechada que deixam um rastro sólido por onde passam. O objetivo é fazer com que os outros competidores colidam contra as paredes ou contra os rastros (incluindo o seu próprio), restando apenas um sobrevivente como vencedor.

---

## 🚀 Funcionalidades

- **Multiplayer em tempo real**: Suporta até 3 jogadores simultâneos conectados via sockets TCP.
- **Arquitetura Cliente-Servidor**:
  - **Servidor**: Gerencia o estado do jogo (posições, arena, colisões) em um loop de jogo sincronizado de *Tick-Rate* (200ms).
  - **Cliente**: Envia as direções inseridas pelo teclado (`W`, `A`, `S`, `D`) de forma assíncrona (usando `select` e modo *raw* de terminal) e renderiza a arena atualizada enviada pelo servidor.
- **Sem IP Dinâmico**: A conexão local é facilitada e configurada por padrão para `127.0.0.1`.
- **Mecânica de Reinício Manual**: A rodada não reinicia sozinha; ao fim da partida, qualquer jogador conectado pode pressionar a tecla `R` para recomeçar o jogo.
- **Identidade Oculta**: As informações sobre a identidade específica de qual jogador você é estão ocultas, deixando as partidas mais misteriosas e dinâmicas!

---

## 🎮 Como Jogar

1. **Movimentação**:
   - `W`: Mover para Cima
   - `A`: Mover para a Esquerda
   - `S`: Mover para Baixo
   - `D`: Mover para a Direita
2. **Reinício**: Pressione `R` na tela de fim de rodada para começar uma nova partida.
3. **Desconexão**: Pressione `Q` a qualquer momento para desconectar e fechar o cliente de jogo.

---

## 💻 Como Compilar e Executar

Este projeto foi projetado para ambientes **POSIX** (Linux / macOS / WSL no Windows) devido ao uso de bibliotecas do sistema como `<sys/socket.h>` e `<termios.h>`.

### 1. Compilação
Abra o seu terminal no diretório do projeto e compile o servidor e o cliente usando o GCC:

```bash
# Compilar o servidor
gcc servidor.c -o servidor

# Compilar o cliente
gcc cliente.c -o cliente
```

### 2. Executando o Servidor
Inicie o servidor primeiro para escutar as conexões dos clientes na porta padrão `8080`:

```bash
./servidor
```

### 3. Executando os Clientes
Abra novos terminais e inicie as instâncias dos clientes (suporta até 3 instâncias para os 3 jogadores):

```bash
./cliente
```
*Por padrão, o cliente tentará se conectar diretamente ao servidor local no endereço `127.0.0.1:8080`.*

---

## 🛠️ Arquitetura do Protocolo

A comunicação de dados entre o cliente e o servidor baseia-se em structs de dados diretas definidas em `protocolo.h`:

- **`ComandoCliente`**: Pacote pequeno enviado pelo cliente a cada comando de tecla com o caractere do movimento (`W`, `A`, `S`, `D`) ou de controle (`R`).
- **`EstadoJogo`**: Pacote completo enviado do servidor para todos os clientes em cada *tick* (200ms) contendo a matriz de 40x20 da arena, a posição/status de todos os jogadores, estado da partida e estatísticas.
