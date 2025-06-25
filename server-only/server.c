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
#include <time.h>        // Para timestamps

#define BUFFER_SIZE 1024 // Tamanho do buffer de mensagens
#define NICKNAME_MAX 50

// Variável global para sinalizar o fim da conexão para as threads
volatile int CONNECTION_ENDED = 0;

// Variáveis globais para nicknames
char server_nickname[NICKNAME_MAX] = "Servidor";
char client_nickname[NICKNAME_MAX] = "Cliente";

// Função para obter timestamp atual
char* obter_timestamp() {
    static char timestamp[20];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M", t);
    return timestamp;
}

// Função para processar comandos do servidor
int processar_comando_servidor(char *mensagem) {
    // Remover quebra de linha se existir
    mensagem[strcspn(mensagem, "\n")] = 0;
    
    if (strcmp(mensagem, "/quit") == 0) {
        printf("\n\033[33m[SISTEMA] Encerrando o chat...\033[0m\n");
        return 1; // Sinalizar para sair
    } else if (strcmp(mensagem, "/help") == 0) {
        printf("\n\033[33m══════════════════════════════════════════════════════════════\033[0m\n");
        printf("\033[33m                           COMANDOS DO CHAT                   \033[0m\n");
        printf("\033[33m══════════════════════════════════════════════════════════════\033[0m\n");
        printf("\033[36m• /quit            \033[0m- Sair do chat\n");
        printf("\033[36m• /help            \033[0m- Mostrar esta ajuda\n");
        printf("\033[36m• /status          \033[0m- Mostrar seu status\n");
        printf("\033[36m• /nick <nome>     \033[0m- Trocar seu nickname\n");
        printf("\033[33m══════════════════════════════════════════════════════════════\033[0m\n\n");
        return 2; // Sinalizar que é comando interno (não enviar)
    } else if (strcmp(mensagem, "/status") == 0) {
        printf("\n\033[34m══════════════════════════════════════════════════════════════\033[0m\n");
        printf("\033[34m                           SEU STATUS                         \033[0m\n");
        printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n");
        printf("\033[32m✓ Nickname: %s\033[0m\n", server_nickname);
        printf("\033[32m✓ Cliente: %s\033[0m\n", client_nickname);
        printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n\n");
        return 2; // Sinalizar que é comando interno (não enviar)
    }
    
    return 0; // Mensagem normal (enviar)
}

// Função executada pela thread de recebimento de mensagens
void *receive_messages_handler(void *socket_descriptor) {
    int client_socket = *(int*)socket_descriptor;
    char peer_message[BUFFER_SIZE];
    int bytes_read;

    // Loop para receber mensagens do cliente
    while ((bytes_read = recv(client_socket, peer_message, BUFFER_SIZE, 0)) > 0) {
        peer_message[bytes_read] = '\0'; // Adiciona terminador de string
        
        // Verificar se é um comando /nick do cliente
        char cmd[BUFFER_SIZE];
        char arg1[BUFFER_SIZE];
        if (sscanf(peer_message, "%s %s", cmd, arg1) >= 1) {
            if (strcmp(cmd, "/nick") == 0 && strlen(arg1) > 0) {
                strncpy(client_nickname, arg1, NICKNAME_MAX - 1);
                client_nickname[NICKNAME_MAX - 1] = '\0';
                printf("\033[33m[SISTEMA] %s alterou o nickname para: %s\033[0m\n", client_nickname, arg1);
                printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
                fflush(stdout);
                continue;
            }
        }
        
        // Exibir mensagem normal do cliente
        printf("\033[32m[%s] %s: %s\033[0m", obter_timestamp(), client_nickname, peer_message);
        if (peer_message[strlen(peer_message) - 1] != '\n') {
            printf("\n");
        }
        printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
        fflush(stdout);

        // Verifica se a mensagem é o comando para encerrar
        if (strncmp(peer_message, "/quit", 5) == 0) {
            break;
        }
    }

    // Se o loop terminar, a conexão foi perdida ou encerrada
    if (bytes_read == 0) {
        printf("\n\033[33m[SISTEMA] Cliente desconectou.\033[0m\n");
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

    printf("\033[32m══════════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[32m                    CHAT PRIVADO - SERVIDOR                    \033[0m\n");
    printf("\033[32m              Aguardando conexão na porta %d              \033[0m\n", port);
    printf("\033[32m              Nickname: %s%-*s              \033[0m\n", server_nickname, 30 - strlen(server_nickname), "");
    printf("\033[32m                                                              \033[0m\n");
    printf("\033[32m  Digite '/quit' para sair                                    \033[0m\n");
    printf("\033[32m══════════════════════════════════════════════════════════════\033[0m\n\n");

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
    printf("\033[32m[SISTEMA] Conexão aceita de %s. Pode começar a conversar.\033[0m\n", peer_ip);
    printf("\033[32m[SISTEMA] Digite '/quit' para encerrar a conversa.\033[0m\n\n");

    // 5. Criar a thread para receber mensagens
    if (pthread_create(&receive_thread, NULL, receive_messages_handler, (void*)&client_socket) < 0) {
        perror("[ERRO] Não foi possível criar a thread de recebimento");
        close(client_socket);
        close(server_socket);
        return 1;
    }

    // 6. Loop principal para enviar mensagens
    char message[BUFFER_SIZE];
    printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
    fflush(stdout);
    
    while (!CONNECTION_ENDED) {
        if (fgets(message, BUFFER_SIZE, stdin) != NULL) {
            // Verificar se é comando /nick
            char cmd[BUFFER_SIZE];
            char arg1[BUFFER_SIZE];
            if (sscanf(message, "%s %s", cmd, arg1) >= 1) {
                if (strcmp(cmd, "/nick") == 0) {
                    if (strlen(arg1) > 0) {
                        // Atualizar nickname local
                        strncpy(server_nickname, arg1, NICKNAME_MAX - 1);
                        server_nickname[NICKNAME_MAX - 1] = '\0';
                        // Enviar para o cliente
                        if (send(client_socket, message, strlen(message), 0) < 0) {
                            perror("[ERRO] Falha ao enviar mensagem");
                            CONNECTION_ENDED = 1;
                        } else {
                            printf("\033[32m✓ Nickname alterado para: %s\033[0m\n", server_nickname);
                            printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
                            fflush(stdout);
                        }
                        continue;
                    } else {
                        printf("\033[31m✗ Uso: /nick <nome>\033[0m\n");
                        printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
                        fflush(stdout);
                        continue;
                    }
                }
            }
            
            // Processar outros comandos
            int resultado_comando = processar_comando_servidor(message);
            if (resultado_comando == 1) {
                // Enviar /quit para o cliente antes de sair
                send(client_socket, "/quit\n", 6, 0);
                CONNECTION_ENDED = 1;
                break;
            } else if (resultado_comando == 2) {
                printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
                fflush(stdout);
                continue;
            }

            // Enviar mensagem normal para o cliente
            if (send(client_socket, message, strlen(message), 0) < 0) {
                perror("[ERRO] Falha ao enviar mensagem");
                CONNECTION_ENDED = 1;
            } else {
                // Exibir mensagem enviada
                printf("\033[34m[%s] %s(você): %s\033[0m", obter_timestamp(), server_nickname, message);
                if (message[strlen(message) - 1] != '\n') {
                    printf("\n");
                }
                printf("[%s] %s(você): ", obter_timestamp(), server_nickname);
                fflush(stdout);
            }
        }
    }

    // Espera a thread de recebimento finalizar
    pthread_join(receive_thread, NULL);

    printf("\n\033[33m[SISTEMA] Encerrando a conexão.\033[0m\n");
    close(client_socket);
    close(server_socket);

    return 0;
}
