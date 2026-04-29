#ifndef ARENA_H
#define ARENA_H

#define MAX_PLAYER 100

#define SHM_KEY 1234
#define MSG_KEY 5678
#define SEM_KEY 9999

typedef struct {
    char username[50];
    char password[50];

    int logged_in;
    int in_battle;

    int hp;
    int damage;
    int gold;

    int opponent;
    int pid;
    int is_bot;
} Player;

typedef struct {
    Player players[MAX_PLAYER];
    int player_count;
    int waiting_player;
} Arena;

typedef struct {
    long type;
    long sender_pid;

    int action;
    int player_id;

    char username[50];
    char password[50];
    char data[100];
} Message;

enum {
    REGISTER,
    LOGIN,
    FIND_MATCH,
    ATTACK
};

#endif
