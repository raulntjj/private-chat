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
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define PROMPT_LENGTH 25 // Tamanho do prompt "[HH:MM] Você: "
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

// Variável global para o nickname
char nickname[NICKNAME_MAX] = "";

// Variável global para armazenar o IP do servidor
char *server_ip_global = NULL;

// Variável global para armazenar o nickname do parceiro
char nickname_parceiro[NICKNAME_MAX] = "Parceiro";

// Função para obter timestamp atual em horário de Brasília
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

// Função para salvar o input atual
void salvar_input_atual() {
    if (posicao_atual > 0) {
        // Mover cursor para o início da linha e salvar o que está sendo digitado
        printf("\r");
        fflush(stdout);
        input_visivel = 1;
    }
}

// Função para restaurar o input atual
void restaurar_input_atual() {
    if (input_visivel && posicao_atual > 0) {
        printf("\033[36m[%s] %s(você): \033[0m%s", obter_timestamp(), nickname, input_atual);
        fflush(stdout);
    }
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

// Função para exibir o banner do lobby
void exibir_banner_lobby() {
    printf("\033[35m════════════════════════════════════════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[35m           CHAT PRIVADO - LOBBY\033[0m\n");
    printf("\033[35m\n");
    printf("  Bem-vindo ao sistema de chat privado!\n");
    printf("  Configure seu perfil antes de iniciar a conversa.\n");
    printf("\n");
    printf("\033[35m════════════════════════════════════════════════════════════════════════════════════════════\033[0m\n\n");
}

// Função para exibir comandos disponíveis
void exibir_comandos() {
    printf("\033[33m══════════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[33m                           COMANDOS                          \033[0m\n");
    printf("\033[33m══════════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[36m• /nick <nome>     \033[0m- Definir seu nickname\n");
    printf("\033[36m• /status          \033[0m- Mostrar status atual\n");
    printf("\033[36m• /quit            \033[0m- Sair do programa\n");
    printf("\033[33m══════════════════════════════════════════════════════════════\033[0m\n\n");
}

// Função para exibir status atual
void exibir_status(const char* ip, int porta) {
    printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[34m                           STATUS                            \033[0m\n");
    printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n");
    if (strlen(nickname) > 0) {
        printf("\033[32m✓ Nickname: %s\033[0m\n", nickname);
    } else {
        printf("\033[31m✗ Nickname: Não definido\033[0m\n");
    }
    printf("\033[32m✓ Servidor: %s:%d\033[0m\n", ip, porta);
    printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n\n");
}

// Função para processar comandos do lobby
int processar_comando_lobby(char *comando, const char* ip, int porta) {
    char cmd[BUFFER_SIZE];
    char arg1[BUFFER_SIZE];
    comando[strcspn(comando, "\n")] = 0;
    if (sscanf(comando, "%s %s", cmd, arg1) >= 1) {
        if (strcmp(cmd, "/nick") == 0) {
            if (strlen(arg1) > 0) {
                strncpy(nickname, arg1, NICKNAME_MAX - 1);
                nickname[NICKNAME_MAX - 1] = '\0';
                printf("\033[32m✓ Nickname definido como: %s\033[0m\n", nickname);
            } else {
                printf("\033[31m✗ Uso: /nick <nome>\033[0m\n");
            }
            return 0;
        } else if (strcmp(cmd, "/status") == 0) {
            exibir_status(ip, porta);
            return 0;
        } else if (strcmp(cmd, "/quit") == 0) {
            printf("\033[33mSaindo do chat...\033[0m\n");
            return 1;
        } else {
            printf("\033[31m✗ Comando não reconhecido. Digite /status para ver informações.\033[0m\n");
            return 0;
        }
    }
    return 0;
}

// Função para executar o lobby
int executar_lobby(const char* ip, int porta) {
    char comando[BUFFER_SIZE];
    printf("\n");
    fflush(stdout);
    exibir_banner_lobby();
    exibir_comandos();
    exibir_status(ip, porta);
    printf("\033[36mDigite um comando ou pressione Enter para iniciar o chat: \033[0m");
    fflush(stdout);
    while (1) {
        if (fgets(comando, BUFFER_SIZE, stdin) != NULL) {
            // Se só apertar Enter, sai do lobby
            if (comando[0] == '\n' || comando[0] == '\0') {
                return 1;
            }
            int resultado = processar_comando_lobby(comando, ip, porta);
            if (resultado == 1) {
                return 0; // Sair do programa
            }
            printf("\033[36mDigite um comando ou pressione Enter para iniciar o chat: \033[0m");
            fflush(stdout);
        }
    }
}

// Função executada pela thread de recebimento de mensagens
void *receber_mensagens(void *socket_desc) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    int sock = *(int*)socket_desc;
    char server_message[BUFFER_SIZE];
    int read_size;

    while ((read_size = recv(sock, server_message, BUFFER_SIZE, 0)) > 0) {
        server_message[read_size] = '\0';
        
        // Verificar se é um comando /nick do parceiro
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

// Função para processar comandos durante o chat
int processar_comando_chat(char *mensagem) {
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
        printf("\033[32m✓ Conectado a: %s\033[0m\n", server_ip_global);
        printf("\033[32m✓ Parceiro: %s\033[0m\n", nickname_parceiro);
        printf("\033[34m══════════════════════════════════════════════════════════════\033[0m\n\n");
        return 2; // Sinalizar que é comando interno (não enviar)
    }
    
    return 0; // Mensagem normal (enviar)
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
    int sock;
    struct sockaddr_in server_addr;
    pthread_t thread_recebimento;
    char* ip;
    int port;

    setenv("TZ", "America/Sao_Paulo", 1);
    tzset();
    
    // Configurar horário de Brasília (UTC-3)
    putenv("TZ=UTC-3");
    tzset();

    if (argc < 3) {
        fprintf(stderr, "Uso: %s <ip_servidor> <porta>\n", argv[0]);
        return 1;
    }
    ip = argv[1];
    port = atoi(argv[2]);
    
    // Inicializar variável global do IP do servidor
    server_ip_global = ip;

    // Lobby amigável antes de conectar
    int lobby_result = executar_lobby(ip, port);
    if (lobby_result == 0) {
        printf("\033[33mSaindo do programa...\033[0m\n");
        return 0;
    }

    configurar_entrada_nao_bloqueante();
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("[ERRO] Não foi possível criar o socket");
        restaurar_terminal();
        return 1;
    }
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[ERRO] Conexão falhou");
        close(sock);
        restaurar_terminal();
        return 1;
    }
    printf("\033[32m══════════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[32m                    CHAT PRIVADO                              \033[0m\n");
    printf("\033[32m              Conectado ao servidor %s:%d              \033[0m\n", ip, port);
    printf("\033[32m              Nickname: %s%-*s              \033[0m\n", nickname, 30 - strlen(nickname), "");
    printf("\033[32m                                                              \033[0m\n");
    printf("\033[32m  Digite '/quit' para sair                                    \033[0m\n");
    printf("\033[32m══════════════════════════════════════════════════════════════\033[0m\n\n");
    if (pthread_create(&thread_recebimento, NULL, receber_mensagens, (void*)&sock) < 0) {
        perror("[ERRO] Não foi possível criar a thread de recebimento");
        close(sock);
        restaurar_terminal();
        return 1;
    }
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
            // Se for comando, processa normalmente
            if (msg_trim[0] == '/' && strlen(msg_trim) > 0) {
                // Verificar se é comando /nick
                char cmd[BUFFER_SIZE];
                char arg1[BUFFER_SIZE];
                if (sscanf(msg_trim, "%s %s", cmd, arg1) >= 1) {
                    if (strcmp(cmd, "/nick") == 0) {
                        if (strlen(arg1) > 0) {
                            // Atualizar nickname local
                            strncpy(nickname, arg1, NICKNAME_MAX - 1);
                            nickname[NICKNAME_MAX - 1] = '\0';
                            // Enviar para o servidor
                            if (send(sock, msg_trim, strlen(msg_trim), 0) < 0) {
                                perror("[ERRO] Falha ao enviar mensagem");
                                FIM_CONEXAO = 1;
                            } else {
                                limpar_linha_atual();
                                printf("\033[32m✓ Nickname alterado para: %s\033[0m\n", nickname);
                                exibir_prompt();
                            }
                        } else {
                            limpar_linha_atual();
                            printf("\033[31m✗ Uso: /nick <nome>\033[0m\n");
                            exibir_prompt();
                        }
                        continue;
                    }
                }
                
                int resultado_comando = processar_comando_chat(msg_trim);
                if (resultado_comando == 1) {
                    FIM_CONEXAO = 1;
                    break;
                } else if (resultado_comando == 2) {
                    exibir_prompt();
                }
            }
            // Só envia/exibe se não for vazio
            else if (strlen(msg_trim) > 0) {
                if (send(sock, msg_trim, strlen(msg_trim), 0) < 0) {
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
    pthread_cancel(thread_recebimento);
    pthread_join(thread_recebimento, NULL);
    printf("\n\033[33m[SISTEMA] Encerrando a conexão...\033[0m\n");
    close(sock);
    restaurar_terminal();
    exit(0);
}