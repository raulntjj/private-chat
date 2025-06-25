// ============================================================================
// DISCIPLINA: Sistemas Distribuídos
// TRABALHO PRÁTICO – Chat (Modelo A: Conversa Privada)
// ARQUIVO: cliente.c
//
// DESCRIÇÃO: Este programa atua como o lado "cliente" do chat.
//            Ele se conecta a um servidor em um IP e porta específicos
//            e então inicia a troca de mensagens bidirecional usando threads.
//
// COMO COMPILAR: gcc cliente.c -o cliente -pthread
// COMO EXECUTAR: ./cliente <ip_servidor> <porta>
//
// Exemplo: ./cliente 127.0.0.1 8080
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

// Variável global para sinalizar o fim da conexão para as threads
volatile int FIM_CONEXAO = 0;

// Função executada pela thread de recebimento de mensagens (idêntica à do servidor)
void *receber_mensagens(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char server_message[BUFFER_SIZE];
    int read_size;

    while ((read_size = recv(sock, server_message, BUFFER_SIZE, 0)) > 0) {
        server_message[read_size] = '\0';
        printf("\n[Parceiro]: %s", server_message);
        fflush(stdout);

        if (strncmp(server_message, "/quit", 5) == 0) {
            break;
        }
    }

    if (read_size == 0) {
        printf("\n[SISTEMA] Parceiro desconectou.\n");
    } else if (read_size == -1) {
        perror("[ERRO] Falha ao receber mensagem");
    }

    FIM_CONEXAO = 1;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <ip_servidor> <porta>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int sock;
    struct sockaddr_in server_addr;
    pthread_t thread_recebimento;

    // 1. Criar o socket do cliente
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("[ERRO] Não foi possível criar o socket");
        return 1;
    }

    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // 2. Conectar ao servidor remoto
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[ERRO] Conexão falhou");
        close(sock);
        return 1;
    }

    printf("[SISTEMA] Conectado ao servidor. Pode começar a conversar.\n");
    printf("[SISTEMA] Digite '/quit' para encerrar a conversa.\n");

    // 3. Criar a thread para receber mensagens
    if (pthread_create(&thread_recebimento, NULL, receber_mensagens, (void*)&sock) < 0) {
        perror("[ERRO] Não foi possível criar a thread de recebimento");
        close(sock);
        return 1;
    }

    // 4. Loop principal para enviar mensagens
    char message[BUFFER_SIZE];
    while (!FIM_CONEXAO) {
        printf("[Você]: ");
        fgets(message, BUFFER_SIZE, stdin);

        if (send(sock, message, strlen(message), 0) < 0) {
            perror("[ERRO] Falha ao enviar mensagem");
            FIM_CONEXAO = 1;
        }

        if (strncmp(message, "/quit", 5) == 0) {
            FIM_CONEXAO = 1;
        }
    }
    
    // Espera a thread de recebimento finalizar
    pthread_join(thread_recebimento, NULL);

    printf("[SISTEMA] Encerrando a conexão.\n");
    close(sock);

    return 0;
}