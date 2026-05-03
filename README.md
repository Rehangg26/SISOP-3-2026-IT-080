# Laporan Praktikum Sistem Operasi  
## Soal 1 – Present Day, Present Time

---

## 1. Deskripsi Soal

Pada soal ini diminta untuk membuat sistem komunikasi berbasis client-server bernama *The Wired*.  
Sistem harus dapat menangani banyak client, memastikan username unik, memungkinkan komunikasi antar client, menyediakan fitur admin, serta mencatat aktivitas ke dalam file log.

---

## 1.1 Implementasi Program

Program terdiri dari beberapa file utama:

- protocol.h  
- protocol.c  
- wired.c  
- navi.c  

---

## 1.2 protocol.h

File ini digunakan untuk mendeklarasikan fungsi yang digunakan oleh client dan server.

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_BUFFER 1024

const char* get_ip();
int get_port();

#endif
````
<img width="587" height="100" alt="ss 1 soal no 1" src="https://github.com/user-attachments/assets/d6be1bf2-bd19-4a10-9397-db7434335419" />

---

## 1.3 protocol.c

File ini digunakan untuk menentukan alamat IP dan port server.

```c
#include "protocol.h"

const char* get_ip() {
    return "127.0.0.1";
}

int get_port() {
    return 8080;
}
```
<img width="706" height="108" alt="ss 2 soal no 1" src="https://github.com/user-attachments/assets/b14e4ef3-4ed6-4ad3-8d27-927e0008294e" />

---

## 1.4 wired.c (Server)

File ini berfungsi sebagai server yang menangani seluruh koneksi client, mengelola username, melakukan broadcast pesan, menyediakan fitur admin, serta mencatat log aktivitas.

```c
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
```
<img width="668" height="133" alt="ss 3 soal no 1" src="https://github.com/user-attachments/assets/0130d915-b470-4341-a9d6-7af56b8da997" />

---

## 1.5 navi.c (Client)

File ini berfungsi sebagai client yang digunakan untuk terhubung ke server, mengirim pesan, dan menerima pesan dari client lain.

```c
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
```
<img width="600" height="180" alt="ss 5 soal no 1" src="https://github.com/user-attachments/assets/b96b4f6f-c9bd-4cdb-8d88-aa4b79c7dadf" />

---

## Hasil Pengujian

### 1.6 Koneksi Client

Client pertama berhasil masuk:

```
Enter your name: Reyhan
--- Welcome to The Wired, Reyhan ---
```

Client kedua:

```
Enter your name: Abam
--- Welcome to The Wired, Abam ---
```

---

### 1.7 Komunikasi Antar Client

<img width="1915" height="507" alt="ss 6 soal no1" src="https://github.com/user-attachments/assets/4abfc0bb-77f9-4969-935b-7d19a7f2c824" />

Hal ini menunjukkan bahwa sistem broadcast berjalan dengan baik.

---

### 1.8 Validasi Username

```
[System] The identity 'Reyhan' is already synchronized in The Wired.
```

Sistem berhasil mencegah penggunaan username yang sama.

---

### 1.9 Mode Admin

Login admin menggunakan:

Username: The Knights
Password: protocol7

Setelah login:

```
=== THE KNIGHTS CONSOLE ===
1. Check Active Entities
2. Check Server Uptime
3. Execute Emergency Shutdown
4. Disconnect
```

---

### 1.10 Logging

Contoh isi file history.log:

```
[2026-04-26 19:07:29] [Admin] [RPC_GET_UPTIME]
```

---

## Kesimpulan

Program berhasil mengimplementasikan sistem komunikasi client-server dengan dukungan multi-client, validasi username, fitur admin, dan logging sesuai dengan kebutuhan soal.

---


## Soal 2 – Multiplayer Battle System

---

##  Deskripsi Soal

Pada soal ini, dibuat sebuah sistem multiplayer berbasis IPC (Inter Process Communication) yang memungkinkan beberapa client untuk saling terhubung dan melakukan pertarungan (battle) secara realtime.

Sistem harus memiliki fitur:
- registrasi dan login
- matchmaking antar player
- battle system
- bot otomatis jika tidak ada lawan
- penggunaan shared memory, message queue, dan semaphore

---

## 2.1 Implementasi Program

Program terdiri dari beberapa file utama:

- arena.h
- orion.c (server)
- eternal.c (client)

---

## 2.2 arena.h

File ini digunakan sebagai struktur utama penyimpanan data.

```c
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
````
<img width="491" height="98" alt="Screenshot 2026-04-28 225003" src="https://github.com/user-attachments/assets/85dd5f72-66ea-4122-b3f2-0c2c7d53c321" />

---

## 2.3 orion.c (Server)
```c
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

```
<img width="566" height="135" alt="Screenshot 2026-04-28 230605" src="https://github.com/user-attachments/assets/55f7b3a5-d47f-46e9-9fe7-332e930414c6" />

Server bertugas:

* menangani registrasi dan login
* melakukan matchmaking
* mengatur battle system
* spawn bot jika tidak ada lawan
* menjaga sinkronisasi menggunakan semaphore

(Server menggunakan shared memory dan message queue untuk komunikasi)

---

## 2.4 eternal.c (Client)
```c
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
```
<img width="584" height="161" alt="Screenshot 2026-04-28 231250" src="https://github.com/user-attachments/assets/f69ccee1-1e82-4b31-ad26-2ace0bc88358" />

Client digunakan oleh user untuk:

* register dan login
* mencari match
* melakukan battle

Client berkomunikasi dengan server melalui message queue.

---

## Hasil Pengujian

### 2.5 Pembuatan Struktur Program

Struktur folder berhasil dibuat:

```
praktikum_sisop3/
├── soal_1/
├── soal_2/
    ├── arena.h
    ├── orion.c
    ├── eternal.c
```

---

### 2.6 Proses Compile

Program berhasil dikompilasi:
<img width="678" height="79" alt="Screenshot 2026-04-28 231341" src="https://github.com/user-attachments/assets/10933e49-0c30-4f5a-ba12-669a02cb2566" />

```bash
gcc orion.c -o orion
gcc eternal.c -o eternal
```

File hasil:

```
arena.h  eternal  eternal.c  orion  orion.c
```

---

### 2.7 Register dan Login

User berhasil melakukan registrasi dan login:
<img width="700" height="485" alt="Screenshot 2026-04-29 203230" src="https://github.com/user-attachments/assets/22b5775c-2611-43b8-8b12-396f200faa44" />

```
[SERVER] Register success
[SERVER] Login success
```

---

### 2.8 Validasi Username

Sistem berhasil menolak username yang sama:
<img width="437" height="214" alt="Screenshot 2026-04-29 203240" src="https://github.com/user-attachments/assets/fa689264-5646-46c2-9e91-50a4dde4f47e" />

```
[SERVER] Username already exists
```

---

### 2.9 Matchmaking

Ketika user memilih match:
<img width="951" height="1019" alt="Screenshot 2026-04-29 203411" src="https://github.com/user-attachments/assets/b2945f2a-3492-4f26-a5fa-84e00243e6fe" />

```
[SERVER] Waiting for opponent...
[SERVER] Match found!
=== BATTLE === tekan A
```

Hal ini menunjukkan bahwa sistem matchmaking berjalan dengan baik.

---

### 2.10 Battle System

Saat battle dimulai:
<img width="957" height="1018" alt="Screenshot 2026-04-29 203427" src="https://github.com/user-attachments/assets/1047477e-1cad-466f-8abc-202d2944e9c4" />

```
[SERVER] Your HP: 90
[SERVER] Enemy HP: 90
```

HP kedua player berkurang sesuai serangan.

---

### 2.11 Battle Berjalan Realtime

HP terus berubah selama battle:
<img width="959" height="1018" alt="Screenshot 2026-04-29 203549" src="https://github.com/user-attachments/assets/2b6bbf31-73de-4736-a8ed-03220060c32f" />

```
[SERVER] Your HP: 80
[SERVER] Enemy HP: 80
[SERVER] Your HP: 70
[SERVER] Enemy HP: 70
```

Hal ini menunjukkan bahwa sistem battle berjalan secara realtime.

---

### 2.12 Kondisi Kalah

Jika HP habis:

```
[SERVER] You LOSE!
```

---

### 2.13 Kondisi Menang

Jika menang:

```
[SERVER] You WIN! Gold +100
```

Sistem juga memberikan reward berupa gold.

---

## Analisis

Dari hasil pengujian:

* sistem IPC berjalan dengan baik
* multiple client dapat terhubung tanpa konflik
* matchmaking berhasil mempertemukan player
* battle system berjalan secara realtime
* semaphore berhasil mencegah race condition

---

## Kesimpulan

Program berhasil mengimplementasikan sistem multiplayer berbasis IPC sesuai dengan kebutuhan soal.

Fitur utama seperti registrasi, login, matchmaking, battle system, dan reward telah berjalan dengan baik dan stabil.

---

```
