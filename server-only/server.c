// ============================================================================
// DISCIPLINA: Sistemas Distribuídos
// TRABALHO PRÁTICO – Chat (Modelo A: Conversa Privada)
// ARQUIVO: server.c
//
// DESCRIÇÃO: Este programa atua como o lado "servidor" do chat.
//            Ele abre uma porta, aguarda uma única conexão de um cliente,
//            e então inicia a troca de mensagens bidirecional usando threads.
//
// COMO COMPILAR: gcc server.c -o server -pthread
// COMO EXECUTAR: ./server <porta>
//
// Exemplo: ./server 8080
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // Para a função close() e write()
#include <pthread.h>     // Para threads
#include <sys/socket.h>  // Para a API de Sockets
#include <arpa/inet.h>   // Para estruturas de endereço

#define BUFFER_SIZE 1024 // Tamanho do buffer de mensagens

// Variável global para sinalizar o fim da conexão para as threads
volatile int CONNECTION_ENDED = 0;

// Função executada pela thread de recebimento de mensagens
void *receive_messages_handler(void *socket_descriptor) {
    int client_socket = *(int*)socket_descriptor;
    char peer_message[BUFFER_SIZE];
    int bytes_read;

    // Loop para receber mensagens do cliente
    while ((bytes_read = recv(client_socket, peer_message, BUFFER_SIZE, 0)) > 0) {
        peer_message[bytes_read] = '\0'; // Adiciona terminador de string
        printf("\n[Parceiro]: %s", peer_message);
        fflush(stdout); // Garante que a mensagem seja impressa imediatamente

        // Verifica se a mensagem é o comando para encerrar
        if (strncmp(peer_message, "/quit", 5) == 0) {
            break;
        }
    }

    // Se o loop terminar, a conexão foi perdida ou encerrada
    if (bytes_read == 0) {
        printf("\n[SISTEMA] Parceiro desconectou.\n");
    } else if (bytes_read == -1) {
        perror("[ERRO] Falha ao receber mensagem");
    }

    CONNECTION_ENDED = 1; // Sinaliza para a thread principal encerrar
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    pthread_t receive_thread;

    // 1. Criar o socket do servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("[ERRO] Não foi possível criar o socket");
        return 1;
    }

    // Preparar a estrutura sockaddr_in
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer IP
    server_address.sin_port = htons(port);       // Porta para escuta

    // 2. Vincular o socket à porta (Bind)
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("[ERRO] Bind falhou");
        close(server_socket);
        return 1;
    }

    // 3. Escutar por conexões (Listen)
    listen(server_socket, 1); // Aceita apenas uma conexão pendente

    printf("[SISTEMA] Aguardando conexão na porta %d...\n", port);

    // 4. Aceitar a conexão (Accept)
    socklen_t client_address_len = sizeof(struct sockaddr_in);
    client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
    if (client_socket < 0) {
        perror("[ERRO] Accept falhou");
        close(server_socket);
        return 1;
    }

    // Obtém e exibe o IP do cliente conectado
    char peer_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, peer_ip, INET_ADDRSTRLEN);
    printf("[SISTEMA] Conexão aceita de %s. Pode começar a conversar.\n", peer_ip);
    printf("[SISTEMA] Digite '/quit' para encerrar a conversa.\n");

    // 5. Criar a thread para receber mensagens
    if (pthread_create(&receive_thread, NULL, receive_messages_handler, (void*)&client_socket) < 0) {
        perror("[ERRO] Não foi possível criar a thread de recebimento");
        close(client_socket);
        close(server_socket);
        return 1;
    }

    // 6. Loop principal para enviar mensagens
    char message[BUFFER_SIZE];
    while (!CONNECTION_ENDED) {
        printf("[Você]: ");
        fgets(message, BUFFER_SIZE, stdin);

        // Envia a mensagem para o cliente
        if (send(client_socket, message, strlen(message), 0) < 0) {
            perror("[ERRO] Falha ao enviar mensagem");
            CONNECTION_ENDED = 1; // Encerra o loop se o envio falhar
        }

        // Se o usuário digitou /quit, encerra a conexão
        if (strncmp(message, "/quit", 5) == 0) {
            CONNECTION_ENDED = 1;
        }
    }

    // Espera a thread de recebimento finalizar
    pthread_join(receive_thread, NULL);

    printf("[SISTEMA] Encerrando a conexão.\n");
    close(client_socket);
    close(server_socket);

    return 0;
}
