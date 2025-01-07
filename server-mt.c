#include "commom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFSZ 1024
#define MAX_CLIENTS 100

// Lista global de clientes
int client_sockets[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

// Função para logar mensagens no formato padrão
void log_message(const char *type, int x, int y, float measurement) {
    printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\n\n", type, x, y, measurement);
}

// Função para gerenciar cada cliente
void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    struct sensor_message msg;

    // Receber dados do cliente
    while (recv(client_socket, &msg, sizeof(msg), 0) > 0) {
        // Log da mensagem recebida
        log_message(msg.type, msg.coords[0], msg.coords[1], msg.measurement);

        // Encaminhar mensagem para outros clientes do mesmo tipo
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] != client_socket) {
                send(client_sockets[i], &msg, sizeof(msg), 0);
            }
        }
        pthread_mutex_unlock(&client_mutex);
    }

    // Cliente desconectado
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == client_socket) {
            close(client_socket);
            client_sockets[i] = client_sockets[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: %s <v4|v6> <server port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage storage;
    if (server_sockaddr_init(argv[1], argv[2], &storage) != 0) {
        printf("Invalid address/port\n");
        exit(EXIT_FAILURE);
    }

    int server_socket = socket(storage.ss_family, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (bind(server_socket, (struct sockaddr *)&storage, sizeof(storage)) != 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) != 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening...\n");

    while (1) {
        struct sockaddr_storage client_storage;
        socklen_t client_len = sizeof(client_storage);
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_storage, &client_len);
        if (*client_socket == -1) {
            perror("accept");
            free(client_socket);
            continue;
        }

        // Adicionar cliente à lista de sockets
        pthread_mutex_lock(&client_mutex);
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = *client_socket;
            pthread_t tid;
            pthread_create(&tid, NULL, client_handler, client_socket);
        } else {
            printf("Maximum clients connected. Connection refused.\n");
            close(*client_socket);
            free(client_socket);
        }
        pthread_mutex_unlock(&client_mutex);
    }

    close(server_socket);
    return 0;
}
