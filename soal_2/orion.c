#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>
#include "arena.h"

Arena *arena;
int semid;
time_t waiting_time = 0;

// ===== LOCK =====
void lock() {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

void unlock() {
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}

int valid(int id) {
    return id >= 0 && id < arena->player_count;
}

// ===== REGISTER =====
void handle_register(Message *msg, int msqid) {
    Message res = {0};
    res.type = msg->sender_pid;

    lock();

    if (arena->player_count >= MAX_PLAYER) {
        unlock();
        sprintf(res.data, "Server full");
        msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
        return;
    }

    for (int i = 0; i < arena->player_count; i++) {
        if (strcmp(arena->players[i].username, msg->username) == 0) {
            unlock();
            sprintf(res.data, "Username already exists");
            msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
            return;
        }
    }

    Player *p = &arena->players[arena->player_count++];
    memset(p, 0, sizeof(Player));

    strcpy(p->username, msg->username);
    strcpy(p->password, msg->password);

    p->hp = 100;
    p->damage = 10;
    p->gold = 150;
    p->opponent = -1;

    unlock();

    sprintf(res.data, "Register success");
    msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
}

// ===== LOGIN =====
void handle_login(Message *msg, int msqid) {
    Message res = {0};
    res.type = msg->sender_pid;

    lock();

    for (int i = 0; i < arena->player_count; i++) {
        Player *p = &arena->players[i];

        if (!strcmp(p->username, msg->username) &&
            !strcmp(p->password, msg->password)) {

            if (p->logged_in) {
                unlock();
                sprintf(res.data, "Already logged in");
                msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
                return;
            }

            p->logged_in = 1;
            p->pid = msg->sender_pid;
            res.player_id = i;

            unlock();

            sprintf(res.data, "Login success");
            msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
            return;
        }
    }

    unlock();

    sprintf(res.data, "Login failed");
    msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
}

// ===== MATCH =====
void handle_match(Message *msg, int msqid) {
    int id = msg->player_id;

    Message res = {0};
    res.type = msg->sender_pid;

    lock();

    if (!valid(id) || !arena->players[id].logged_in) {
        unlock();
        return;
    }

    if (arena->waiting_player == -1) {
        arena->waiting_player = id;
        waiting_time = time(NULL);

        unlock();

        sprintf(res.data, "Waiting for opponent...");
        msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);
    } else {
        int opp = arena->waiting_player;

        if (!valid(opp) || opp == id) {
            arena->waiting_player = -1;
            unlock();
            return;
        }

        arena->players[id].in_battle = 1;
        arena->players[opp].in_battle = 1;

        arena->players[id].opponent = opp;
        arena->players[opp].opponent = id;

        int opp_pid = arena->players[opp].pid;

        arena->waiting_player = -1;

        unlock();

        sprintf(res.data, "Match found!");
        msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);

        Message res2 = {0};
        res2.type = opp_pid;
        sprintf(res2.data, "Match found!");
        msgsnd(msqid, &res2, sizeof(res2)-sizeof(long), 0);
    }
}

// ===== ATTACK =====
void handle_attack(Message *msg, int msqid) {
    int id = msg->player_id;

    lock();

    if (!valid(id)) {
        unlock();
        return;
    }

    int opp = arena->players[id].opponent;

    if (!valid(opp)) {
        unlock();
        return;
    }

    Player *p1 = &arena->players[id];
    Player *p2 = &arena->players[opp];

    if (!p1->in_battle) {
        unlock();
        return;
    }

    p2->hp -= p1->damage;

    if (p2->is_bot && p2->hp > 0) {
        p1->hp -= p2->damage;
    }

    int hp = p2->hp;
    int pid1 = p1->pid;
    int pid2 = p2->pid;

    if (hp <= 0) {
        p1->in_battle = 0;
        p2->in_battle = 0;

        p1->gold += 100;

        p1->opponent = -1;
        p2->opponent = -1;
    }

    unlock();

    Message r1 = {0}, r2 = {0};
    r1.type = pid1;
    r2.type = pid2;

    if (hp <= 0) {
        sprintf(r1.data, "You WIN! Gold +100");
        if (!p2->is_bot)
            sprintf(r2.data, "You LOSE!");
    } else {
        sprintf(r1.data, "Enemy HP: %d", hp);
        if (!p2->is_bot)
            sprintf(r2.data, "Your HP: %d", hp);
    }

    msgsnd(msqid, &r1, sizeof(r1)-sizeof(long), 0);
    if (!p2->is_bot)
        msgsnd(msqid, &r2, sizeof(r2)-sizeof(long), 0);
}

// ===== MAIN =====
int main() {
    int shmid = shmget(SHM_KEY, sizeof(Arena), IPC_CREAT | 0666);
    arena = (Arena*) shmat(shmid, NULL, 0);

    int msqid = msgget(MSG_KEY, IPC_CREAT | 0666);

    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    semctl(semid, 0, SETVAL, 1);

    arena->player_count = 0;
    arena->waiting_player = -1;

    printf("Orion running...\n");

    Message msg;

    while (1) {

        // ===== BOT SPAWN =====
        lock();

        if (arena->waiting_player != -1 &&
            time(NULL) - waiting_time >= 35) {

            if (arena->player_count >= MAX_PLAYER) {
                unlock();
                continue;
            }

            int id = arena->waiting_player;

            Player *bot = &arena->players[arena->player_count++];
            memset(bot, 0, sizeof(Player));

            strcpy(bot->username, "BOT");
            bot->hp = 100;
            bot->damage = 5;
            bot->is_bot = 1;

            arena->players[id].in_battle = 1;
            arena->players[id].opponent = arena->player_count - 1;

            bot->in_battle = 1;
            bot->opponent = id;

            int pid = arena->players[id].pid;

            arena->waiting_player = -1;

            unlock();

            Message res = {0};
            res.type = pid;
            sprintf(res.data, "Matched with BOT!");
            msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);

        } else {
            unlock();
        }

        msgrcv(msqid, &msg, sizeof(msg)-sizeof(long), 1, 0);

        switch (msg.action) {
            case REGISTER: handle_register(&msg, msqid); break;
            case LOGIN: handle_login(&msg, msqid); break;
            case FIND_MATCH: handle_match(&msg, msqid); break;
            case ATTACK: handle_attack(&msg, msqid); break;
        }
    }
}
