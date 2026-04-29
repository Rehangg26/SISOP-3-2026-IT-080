#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "protocol.h"

#define MAX_CLIENTS 100
#define ADMIN_NAME "The Knights"
#define ADMIN_PASS "protocol7"

typedef struct {
    int socket;
    char name[50];
    int is_admin;
} client_t;

client_t clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t lock;

// ===== LOG (FIXED: no silent fail) =====
void write_log(const char *type, const char *msg) {
    FILE *f = fopen("history.log", "a");

    if (!f) {
        perror("LOG ERROR");  // penting
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s]\n",
        t->tm_year+1900, t->tm_mon+1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        type, msg);

    fclose(f);
}

// ===== CHECK USERNAME =====
int is_name_exist(char *name) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, name) == 0)
            return 1;
    }
    return 0;
}

// ===== BROADCAST =====
void broadcast(char *msg, int sender) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != sender) {
            send(clients[i].socket, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&lock);
}

// ===== REMOVE CLIENT =====
void remove_client(int sock) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == sock) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

// ===== HANDLE CLIENT =====
void *handle_client(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char name[50], buffer[MAX_BUFFER];
    int is_admin = 0;

    // ===== USERNAME LOOP =====
    while (1) {
        int len = recv(sock, name, sizeof(name), 0);
        if (len <= 0) {
            close(sock);
            return NULL;
        }

        name[strcspn(name, "\n")] = 0;

        pthread_mutex_lock(&lock);
        if (is_name_exist(name)) {
            pthread_mutex_unlock(&lock);

            char msg[200];
            sprintf(msg, "[System] The identity '%s' is already synchronized in The Wired.\n", name);
            send(sock, msg, strlen(msg), 0);
        } else {
            strcpy(clients[client_count].name, name);
            clients[client_count].socket = sock;
            clients[client_count].is_admin = 0;
            client_count++;
            pthread_mutex_unlock(&lock);
            break;
        }
    }

    // ===== LOG CONNECT =====
    char logmsg[200];
    sprintf(logmsg, "User '%s' connected", name);
    write_log("System", logmsg);

    // ===== ADMIN LOGIN =====
    if (strcmp(name, ADMIN_NAME) == 0) {
        send(sock, "PASSWORD_REQUIRED\n", 18, 0);

        int len = recv(sock, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            close(sock);
            return NULL;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, ADMIN_PASS) == 0) {
            is_admin = 1;

            pthread_mutex_lock(&lock);
            for (int i = 0; i < client_count; i++) {
                if (clients[i].socket == sock) {
                    clients[i].is_admin = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            char full_msg[1024];
            sprintf(full_msg,
                "[System] Authentication Successful. Granted Admin privileges.\n"
                "\n=== THE KNIGHTS CONSOLE ===\n"
                "1. Check Active Entities (Users)\n"
                "2. Check Server Uptime\n"
                "3. Execute Emergency Shutdown\n"
                "4. Disconnect\n"
                "Command >> ");

            send(sock, full_msg, strlen(full_msg), 0);
        } else {
            send(sock, "[System] Authentication Failed.\n", 32, 0);
            close(sock);
            return NULL;
        }
    } else {
        char welcome[200];
        sprintf(welcome, "--- Welcome to The Wired, %s ---\n", name);
        send(sock, welcome, strlen(welcome), 0);
    }

    // ===== MAIN LOOP =====
    while (1) {
        int len = recv(sock, buffer, sizeof(buffer), 0);
        if (len <= 0) break;

        buffer[len] = '\0';

        if (is_admin) {
            if (strcmp(buffer, "1\n") == 0) {
                write_log("Admin", "RPC_GET_USERS");

                char list[500] = "Users:\n";
                pthread_mutex_lock(&lock);
                for (int i = 0; i < client_count; i++) {
                    strcat(list, clients[i].name);
                    strcat(list, "\n");
                }
                pthread_mutex_unlock(&lock);

                send(sock, list, strlen(list), 0);
            }
            else if (strcmp(buffer, "2\n") == 0) {
                write_log("Admin", "RPC_GET_UPTIME");
                send(sock, "Server is running.\n", 20, 0);
            }
            else if (strcmp(buffer, "3\n") == 0) {
                write_log("Admin", "RPC_SHUTDOWN");
                write_log("System", "EMERGENCY SHUTDOWN INITIATED");
                exit(0);
            }
            else if (strcmp(buffer, "4\n") == 0) {
                break;
            }

            send(sock, "Command >> ", 11, 0);
        } else {
            char msg[1200];
            sprintf(msg, "[%s]: %s", name, buffer);

            write_log("User", msg);
            broadcast(msg, sock);
        }
    }

    // ===== LOG DISCONNECT =====
    sprintf(logmsg, "User '%s' disconnected", name);
    write_log("System", logmsg);

    remove_client(sock);
    close(sock);
    return NULL;
}

// ===== MAIN =====
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    pthread_mutex_init(&lock, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(get_port());

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    write_log("System", "SERVER ONLINE");

    printf("Server running...\n");

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);

        int *client_sock = malloc(sizeof(int));
        *client_sock = new_socket;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_sock);
    }

    return 0;
}