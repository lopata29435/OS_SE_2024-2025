#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 128

const char *keywords[] = {"int", "return", "if", "else", "while"};
#define KEYWORDS_COUNT (sizeof(keywords) / sizeof(keywords[0]))

int process_text(const char *fifo_in, const char *fifo_out) {
    int fifo_in_fd = open(fifo_in, O_RDONLY);
    if (fifo_in_fd < 0) {
        perror("Error opening FIFO for reading");
        return 1;
    }

    int fifo_out_fd = open(fifo_out, O_WRONLY);
    if (fifo_out_fd < 0) {
        perror("Error opening FIFO for writing");
        close(fifo_in_fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int counts[KEYWORDS_COUNT] = {0};

    while ((bytes_read = read(fifo_in_fd, buffer, BUFFER_SIZE)) > 0) {
        buffer[bytes_read] = '\0';
        char *token = strtok(buffer, " \t\n.,;(){}[]<>!+-=*/");
        while (token) {
            for (int i = 0; i < KEYWORDS_COUNT; i++) {
                if (strcmp(token, keywords[i]) == 0) {
                    counts[i]++;
                }
            }
            token = strtok(NULL, " \t\n.,;(){}[]<>!+-=*/");
        }
    }

    write(fifo_out_fd, counts, sizeof(counts));
    close(fifo_in_fd);
    close(fifo_out_fd);
    return 0;
}

int main() {
    const char *fifo1 = "/tmp/fifo1";
    const char *fifo2 = "/tmp/fifo2";

    if (process_text(fifo1, fifo2) != 0) {
        return 1;
    }

    return 0;
}