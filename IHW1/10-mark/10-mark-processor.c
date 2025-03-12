#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define BUFFER_SIZE 128
#define MSG_TYPE_DATA 1
#define MSG_TYPE_RESULT 2

typedef struct {
    long mtype;
    char mtext[BUFFER_SIZE];
} message_t;

void send_message(int msgid, const char *data, size_t size, long type) {
    message_t msg;
    msg.mtype = type;
    memcpy(msg.mtext, data, size);
    if (msgsnd(msgid, &msg, size, 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}

void receive_message(int msgid, char *buffer, size_t size, long type) {
    message_t msg;
    if (msgrcv(msgid, &msg, size, type, 0) == -1) {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    memcpy(buffer, msg.mtext, size);
}

int process_text(int msgid1, int msgid2) {
    const char *keywords[] = {"int", "return", "if", "else", "while"};
    int counts[5] = {0};

    char buffer[BUFFER_SIZE];
    while (1) {
        receive_message(msgid1, buffer, BUFFER_SIZE, MSG_TYPE_DATA);
        if (buffer[0] == '\0') break;
        if (strlen(buffer) == 0) break;

        char *token = strtok(buffer, " \t\n.,;(){}[]<>!+-=*/");
        while (token) {
            for (int i = 0; i < 5; i++) {
                if (strcmp(token, keywords[i]) == 0) {
                    counts[i]++;
                }
            }
            token = strtok(NULL, " \t\n.,;(){}[]<>!+-=*/");
        }
    }

    send_message(msgid2, (char *)counts, sizeof(counts), MSG_TYPE_RESULT);
    return 0;
}

int main() {
    key_t key1 = ftok("msg1", 65);
    key_t key2 = ftok("msg2", 66);

    int msgid1 = msgget(key1, 0666 | IPC_CREAT);
    int msgid2 = msgget(key2, 0666 | IPC_CREAT);

    if (msgid1 == -1 || msgid2 == -1) {
        perror("msgget");
        return 1;
    }

    if (process_text(msgid1, msgid2) != 0) {
        msgctl(msgid1, IPC_RMID, NULL);
        msgctl(msgid2, IPC_RMID, NULL);
        return 1;
    }

    msgctl(msgid1, IPC_RMID, NULL);
    msgctl(msgid2, IPC_RMID, NULL);

    return 0;
}