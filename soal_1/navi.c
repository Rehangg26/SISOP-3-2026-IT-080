#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.h"

int sock;

void *receive_msg(void *arg) {
    char buffer[MAX_BUFFER];

    while (1) {
        int len = recv(sock, buffer, sizeof(buffer), 0);
        if (len <= 0) break;

        buffer[len] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}

void *send_msg(void *arg) {
    char buffer[MAX_BUFFER];

    while (1) {
        fgets(buffer, sizeof(buffer), stdin);
        send(sock, buffer, strlen(buffer), 0);
    }
    return NULL;
}

int main() {
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(get_port());
    inet_pton(AF_INET, get_ip(), &serv_addr.sin_addr);

    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    char name[50], buffer[MAX_BUFFER];

    while (1) {
        printf("Enter your name: ");
        fgets(name, sizeof(name), stdin);
        send(sock, name, strlen(name), 0);

        int len = recv(sock, buffer, sizeof(buffer), 0);
        if (len <= 0) return 0;

        buffer[len] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "Welcome") || strstr(buffer, "PASSWORD_REQUIRED"))
            break;
    }

    if (strstr(buffer, "PASSWORD_REQUIRED")) {
        char pass[50];
        printf("Enter Password: ");
        fgets(pass, sizeof(pass), stdin);
        send(sock, pass, strlen(pass), 0);
    }

    pthread_t t1, t2;
    pthread_create(&t1, NULL, send_msg, NULL);
    pthread_create(&t2, NULL, receive_msg, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    close(sock);
    return 0;
}