#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include "arena.h"

int msqid;
long my_pid;

void send_req(Message *m) {
    msgsnd(msqid, m, sizeof(*m)-sizeof(long), 0);
}

void receive_all() {
    Message m;
    while (msgrcv(msqid, &m, sizeof(m)-sizeof(long), my_pid, IPC_NOWAIT) > 0) {
        printf("[SERVER] %s\n", m.data);
        if (strstr(m.data, "WIN") || strstr(m.data, "LOSE")) exit(0);
    }
}

void battle(int id) {
    printf("=== BATTLE === tekan A\n");

    while (1) {
        char c;
        scanf(" %c",&c);

        if (c=='A') {
            Message m = {0};
            m.type = 1;
            m.sender_pid = my_pid;
            m.player_id = id;
            m.action = ATTACK;

            send_req(&m);
            sleep(1);
            receive_all();
        }
    }
}

void match(int id) {
    Message m = {0};
    m.type = 1;
    m.sender_pid = my_pid;
    m.player_id = id;
    m.action = FIND_MATCH;

    send_req(&m);

    while (1) {
        Message r;
        msgrcv(msqid,&r,sizeof(r)-sizeof(long),my_pid,0);

        printf("[SERVER] %s\n", r.data);

        if (strstr(r.data,"Match") || strstr(r.data,"BOT")) {
            battle(id);
            break;
        }
    }
}

int login(int *id) {
    Message m = {0};
    m.type = 1;
    m.sender_pid = my_pid;
    m.action = LOGIN;

    printf("Username: "); scanf("%s",m.username);
    printf("Password: "); scanf("%s",m.password);

    send_req(&m);

    Message r;
    msgrcv(msqid,&r,sizeof(r)-sizeof(long),my_pid,0);

    printf("[SERVER] %s\n", r.data);

    if (!strcmp(r.data,"Login success")) {
        *id = r.player_id;
        return 1;
    }
    return 0;
}

void regis() {
    Message m = {0};
    m.type = 1;
    m.sender_pid = my_pid;
    m.action = REGISTER;

    printf("Username: "); scanf("%s",m.username);
    printf("Password: "); scanf("%s",m.password);

    send_req(&m);

    Message r;
    msgrcv(msqid,&r,sizeof(r)-sizeof(long),my_pid,0);

    printf("[SERVER] %s\n", r.data);
}

int main() {
    msqid = msgget(MSG_KEY,0666);
    if (msqid==-1) {
        printf("Server belum jalan!\n");
        return 0;
    }

    my_pid = getpid();

    int id=-1,c;

    while(1) {
        printf("\n1.Register\n2.Login\n3.Match\n4.Exit\nChoice: ");
        scanf("%d",&c);

        if (c==1) regis();
        else if (c==2) login(&id);
        else if (c==3) {
            if (id==-1) printf("Login dulu!\n");
            else match(id);
        }
        else exit(0);
    }
}
