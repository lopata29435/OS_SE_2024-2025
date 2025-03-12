#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

int process_file(const char *input_file, int msgid1, int msgid2) {
    struct stat st;
    if (stat(input_file, &st) == -1) {
        perror("Error getting file status");
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is a directory\n", input_file);
        return 1;
    }

    int fd = open(input_file, O_RDONLY);
    if (fd < 0) {
        perror("Error opening input file");
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        send_message(msgid1, buffer, bytes_read, MSG_TYPE_DATA);
    }

    send_message(msgid1, "", 1, MSG_TYPE_DATA); 
    close(fd);
    return 0;
}

int process_output(const char *output_file, int msgid2) {
    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error opening output file");
        return 1;
    }

    int counts[5] = {0};
    receive_message(msgid2, (char *)counts, sizeof(counts), MSG_TYPE_RESULT);

    const char *keywords[] = {"int", "return", "if", "else", "while"};
    char buffer[256];
    for (int i = 0; i < 5; i++) {
        int len = snprintf(buffer, sizeof(buffer), "%s: %d\n", keywords[i], counts[i]);
        write(fd, buffer, len);
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    key_t key1 = ftok("msg1", 65);
    key_t key2 = ftok("msg2", 66);

    int msgid1 = msgget(key1, 0666 | IPC_CREAT);
    int msgid2 = msgget(key2, 0666 | IPC_CREAT);

    if (msgid1 == -1 || msgid2 == -1) {
        perror("msgget");
        return 1;
    }

    if (process_file(argv[1], msgid1, msgid2) != 0) {
        msgctl(msgid1, IPC_RMID, NULL);
        msgctl(msgid2, IPC_RMID, NULL);
        return 1;
    }

    if (process_output(argv[2], msgid2) != 0) {
        msgctl(msgid1, IPC_RMID, NULL);
        msgctl(msgid2, IPC_RMID, NULL);
        return 1;
    }

    msgctl(msgid1, IPC_RMID, NULL);
    msgctl(msgid2, IPC_RMID, NULL);

    return 0;
}