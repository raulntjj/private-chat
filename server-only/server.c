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
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define NICKNAME_MAX 50

// Variável global para sinalizar o fim da conexão para as threads
volatile int FIM_CONEXAO = 0;
volatile int MENSAGEM_RECEBIDA = 0;

// Estrutura para armazenar a mensagem recebida
typedef struct {
    char mensagem[BUFFER_SIZE];
    int tamanho;
} MensagemRecebida;

MensagemRecebida ultima_mensagem;
pthread_mutex_t mutex_mensagem = PTHREAD_MUTEX_INITIALIZER;

// Variáveis globais para gerenciar o input atual
char input_atual[BUFFER_SIZE] = "";
int posicao_atual = 0;
int input_visivel = 0; // Flag para indicar se há input visível na tela

// Variáveis globais para nicknames
char nickname[NICKNAME_MAX] = "Servidor";
char nickname_parceiro[NICKNAME_MAX] = "Cliente";

// Função para obter timestamp atual
char* obter_timestamp() {
    static char timestamp[20];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M", t);
    return timestamp;
}

// Função para limpar linha atual
void limpar_linha_atual() {
    printf("\r\033[K"); // Volta ao início da linha e limpa
    fflush(stdout);
}

// Função para configurar entrada não-bloqueante
void configurar_entrada_nao_bloqueante() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// Função para restaurar configurações do terminal
void restaurar_terminal() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
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
        printf("\033[32m✓ Nickname: %s\033[0m\n", nickname);
        printf("\033[32m✓ Parceiro: %s\033[0m\n", nickname_parceiro);
        printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n\n");
        return 2; // Sinalizar que é comando interno (não enviar)
    }
    
    return 0; // Mensagem normal (enviar)
}

// Função executada pela thread de recebimento de mensagens
void *receber_mensagens(void *socket_desc) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    int sock = *(int*)socket_desc;
    char server_message[BUFFER_SIZE];
    int read_size;

    while ((read_size = recv(sock, server_message, BUFFER_SIZE, 0)) > 0) {
        server_message[read_size] = '\0';
        
        // Verificar se é um comando /nick do cliente
        char cmd[BUFFER_SIZE];
        char arg1[BUFFER_SIZE];
        if (sscanf(server_message, "%s %s", cmd, arg1) >= 1) {
            if (strcmp(cmd, "/nick") == 0 && strlen(arg1) > 0) {
                strncpy(nickname_parceiro, arg1, NICKNAME_MAX - 1);
                nickname_parceiro[NICKNAME_MAX - 1] = '\0';
                pthread_mutex_lock(&mutex_mensagem);
                strcpy(ultima_mensagem.mensagem, server_message);
                ultima_mensagem.tamanho = read_size;
                MENSAGEM_RECEBIDA = 1;
                pthread_mutex_unlock(&mutex_mensagem);
                continue;
            }
        }
        
        pthread_mutex_lock(&mutex_mensagem);
        strcpy(ultima_mensagem.mensagem, server_message);
        ultima_mensagem.tamanho = read_size;
        MENSAGEM_RECEBIDA = 1;
        pthread_mutex_unlock(&mutex_mensagem);
        if (strncmp(server_message, "/quit", 5) == 0) {
            break;
        }
    }
    if (read_size == 0) {
        printf("\n\033[33m[SISTEMA] Parceiro desconectou.\033[0m\n");
    } else if (read_size == -1) {
        perror("[ERRO] Falha ao receber mensagem");
    }
    FIM_CONEXAO = 1;
    return 0;
}

// Função para ler entrada do usuário de forma não-bloqueante
int ler_entrada_usuario(char *buffer, int max_size) {
    char c;
    int bytes_read = read(STDIN_FILENO, &c, 1);
    
    if (bytes_read <= 0) {
        return 0; // Nenhum caractere disponível
    }
    
    if (c == '\n' || c == '\r') {
        // Enter pressionado - finalizar mensagem
        if (posicao_atual > 0) {
            input_atual[posicao_atual] = '\0';
            strcpy(buffer, input_atual);
            posicao_atual = 0;
            input_atual[0] = '\0';
            input_visivel = 0;
            return 1; // Mensagem completa
        }
    } else if (c == 127 || c == 8) {
        // Backspace - só permite apagar se não estiver no início do prompt
        if (posicao_atual > 0) {
            posicao_atual--;
            input_atual[posicao_atual] = '\0';
            printf("\b \b"); // Apagar caractere na tela
            fflush(stdout);
        }
        // Se tentar apagar além do prompt, não faz nada (protege o prompt)
    } else if (posicao_atual < max_size - 1) {
        // Adicionar caractere ao buffer
        input_atual[posicao_atual] = c;
        posicao_atual++;
        input_atual[posicao_atual] = '\0';
        printf("%c", c); // Mostrar caractere na tela
        fflush(stdout);
    }
    
    return 0; // Mensagem ainda não completa
}

// Função para exibir prompt de entrada
void exibir_prompt() {
    printf("\033[36m[%s] %s(você): \033[0m", obter_timestamp(), nickname);
    fflush(stdout);
}

// Função para exibir mensagem recebida
void exibir_mensagem_recebida(const char *mensagem) {
    // Limpa a linha atual do input
    limpar_linha_atual();

    // Verificar se é um comando /nick
    char cmd[BUFFER_SIZE];
    char arg1[BUFFER_SIZE];
    if (sscanf(mensagem, "%s %s", cmd, arg1) >= 1) {
        if (strcmp(cmd, "/nick") == 0 && strlen(arg1) > 0) {
            // Mostrar mensagem de confirmação do nickname do parceiro
            printf("\033[33m[SISTEMA] %s alterou o nickname para: %s\033[0m\n", nickname_parceiro, arg1);
        } else {
            // Exibe a mensagem recebida normal
            printf("\033[32m[%s] %s: %s\033[0m", obter_timestamp(), nickname_parceiro, mensagem);
            if (mensagem[strlen(mensagem) - 1] != '\n') {
                printf("\n");
            }
        }
    } else {
        // Exibe a mensagem recebida normal
        printf("\033[32m[%s] %s: %s\033[0m", obter_timestamp(), nickname_parceiro, mensagem);
        if (mensagem[strlen(mensagem) - 1] != '\n') {
            printf("\n");
        }
    }

    // Sempre reimprime o prompt e o input atual (se houver)
    printf("\033[36m[%s] %s(você): \033[0m%s", obter_timestamp(), nickname, input_atual);
    fflush(stdout);
}

// Função para exibir mensagem enviada
void exibir_mensagem_enviada(const char *mensagem) {
    limpar_linha_atual();
    printf("\033[34m[%s] %s(você): %s\033[0m\n", obter_timestamp(), nickname, mensagem);
    exibir_prompt();
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

    setenv("TZ", "America/Sao_Paulo", 1);
    tzset();

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
    printf("\033[32m              Nickname: %s%-*s              \033[0m\n", nickname, 30 - strlen(nickname), "");
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

    configurar_entrada_nao_bloqueante();

    // 5. Criar a thread para receber mensagens
    if (pthread_create(&receive_thread, NULL, receber_mensagens, (void*)&client_socket) < 0) {
        perror("[ERRO] Não foi possível criar a thread de recebimento");
        close(client_socket);
        close(server_socket);
        restaurar_terminal();
        return 1;
    }

    // 6. Loop principal para enviar mensagens
    char message[BUFFER_SIZE];
    exibir_prompt();
    
    while (!FIM_CONEXAO) {
        if (MENSAGEM_RECEBIDA) {
            pthread_mutex_lock(&mutex_mensagem);
            exibir_mensagem_recebida(ultima_mensagem.mensagem);
            MENSAGEM_RECEBIDA = 0;
            pthread_mutex_unlock(&mutex_mensagem);
        }
        if (ler_entrada_usuario(message, BUFFER_SIZE)) {
            // Remove espaços em branco do início e fim
            char *msg_trim = message;
            while (*msg_trim == ' ' || *msg_trim == '\t') msg_trim++;
            size_t len = strlen(msg_trim);
            while (len > 0 && (msg_trim[len-1] == ' ' || msg_trim[len-1] == '\t' || msg_trim[len-1] == '\n')) {
                msg_trim[--len] = '\0';
            }
            
            // Verificar se é comando /nick
            char cmd[BUFFER_SIZE];
            char arg1[BUFFER_SIZE];
            if (sscanf(msg_trim, "%s %s", cmd, arg1) >= 1) {
                if (strcmp(cmd, "/nick") == 0) {
                    if (strlen(arg1) > 0) {
                        // Atualizar nickname local
                        strncpy(nickname, arg1, NICKNAME_MAX - 1);
                        nickname[NICKNAME_MAX - 1] = '\0';
                        // Enviar para o cliente
                        if (send(client_socket, msg_trim, strlen(msg_trim), 0) < 0) {
                            perror("[ERRO] Falha ao enviar mensagem");
                            FIM_CONEXAO = 1;
                        } else {
                            limpar_linha_atual();
                            printf("\033[32m✓ Nickname alterado para: %s\033[0m\n", nickname);
                            exibir_prompt();
                        }
                        continue;
                    } else {
                        limpar_linha_atual();
                        printf("\033[31m✗ Uso: /nick <nome>\033[0m\n");
                        exibir_prompt();
                        continue;
                    }
                }
            }
            
            // Se for comando, processa normalmente
            if (msg_trim[0] == '/' && strlen(msg_trim) > 0) {
                int resultado_comando = processar_comando_servidor(msg_trim);
                if (resultado_comando == 1) {
                    // Enviar /quit para o cliente antes de sair
                    send(client_socket, "/quit\n", 6, 0);
                    FIM_CONEXAO = 1;
                    break;
                } else if (resultado_comando == 2) {
                    exibir_prompt();
                }
            }
            // Só envia/exibe se não for vazio
            else if (strlen(msg_trim) > 0) {
                if (send(client_socket, msg_trim, strlen(msg_trim), 0) < 0) {
                    perror("[ERRO] Falha ao enviar mensagem");
                    FIM_CONEXAO = 1;
                } else {
                    exibir_mensagem_enviada(msg_trim);
                }
            } else {
                exibir_prompt();
            }
        }
        usleep(10000);
    }

    // Espera a thread de recebimento finalizar
    pthread_cancel(receive_thread);
    pthread_join(receive_thread, NULL);

    printf("\n\033[33m[SISTEMA] Encerrando a conexão.\033[0m\n");
    close(client_socket);
    close(server_socket);
    restaurar_terminal();
    exit(0);
}
