#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER_SIZE 5000

const char *keywords[] = {"int", "return", "if", "else", "while"};
#define KEYWORDS_COUNT (sizeof(keywords) / sizeof(keywords[0]))

void create_fifo(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

int process_file(const char *input_file, const char *fifo_name) {
    int fd = open(input_file, O_RDONLY);
    if (fd < 0) {
        perror("Error opening input file");
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("Error reading input file");
        close(fd);
        return 1;
    }
    buffer[bytes_read] = '\0';

    int fifo_fd = open(fifo_name, O_WRONLY);
    if (fifo_fd < 0) {
        perror("Error opening FIFO for writing");
        close(fd);
        return 1;
    }

    write(fifo_fd, buffer, bytes_read + 1);
    close(fd);
    close(fifo_fd);
    return 0;
}

int process_text(const char *fifo_in, const char *fifo_out) {
    int fifo_in_fd = open(fifo_in, O_RDONLY);
    if (fifo_in_fd < 0) {
        perror("Error opening FIFO for reading");
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(fifo_in_fd, buffer, BUFFER_SIZE);
    if (bytes_read < 0) {
        perror("Error reading from FIFO");
        close(fifo_in_fd);
        return 1;
    }
    close(fifo_in_fd);

    buffer[bytes_read] = '\0';
    int counts[KEYWORDS_COUNT] = {0};
    char *token = strtok(buffer, " \t\n.,;(){}[]<>!+-=*/");
    while (token) {
        for (int i = 0; i < KEYWORDS_COUNT; i++) {
            if (strcmp(token, keywords[i]) == 0) {
                counts[i]++;
            }
        }
        token = strtok(NULL, " \t\n.,;(){}[]<>!+-=*/");
    }

    int fifo_out_fd = open(fifo_out, O_WRONLY);
    if (fifo_out_fd < 0) {
        perror("Error opening FIFO for writing");
        return 1;
    }

    write(fifo_out_fd, counts, sizeof(counts));
    close(fifo_out_fd);
    return 0;
}

int process_output(const char *fifo_name, const char *output_file) {
    int fifo_fd = open(fifo_name, O_RDONLY);
    if (fifo_fd < 0) {
        perror("Error opening FIFO for reading");
        return 1;
    }

    int counts[KEYWORDS_COUNT];
    ssize_t bytes_read = read(fifo_fd, counts, sizeof(counts));
    if (bytes_read < 0) {
        perror("Error reading from FIFO");
        close(fifo_fd);
        return 1;
    }
    close(fifo_fd);

    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error opening output file");
        return 1;
    }

    char buffer[256];
    for (int i = 0; i < KEYWORDS_COUNT; i++) {
        int len = snprintf(buffer, sizeof(buffer), "%s: %d\n", keywords[i], counts[i]);
        write(fd, buffer, len);
    }
    close(fd);
    return 0;
}

void kill_children(pid_t pid1, pid_t pid2, pid_t pid3) {
    if (pid1 > 0) kill(pid1, SIGTERM);
    if (pid2 > 0) kill(pid2, SIGTERM);
    if (pid3 > 0) kill(pid3, SIGTERM);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    const char *fifo1 = "/tmp/fifo1";
    const char *fifo2 = "/tmp/fifo2";

    create_fifo(fifo1);
    create_fifo(fifo2);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        if (process_file(argv[1], fifo1) != 0) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        if (process_text(fifo1, fifo2) != 0) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {
        if (process_output(fifo2, argv[2]) != 0) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    int status;
    waitpid(pid1, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Child process 1 failed\n");
        kill_children(pid1, pid2, pid3);
        unlink(fifo1);
        unlink(fifo2);
        return 1;
    }

    waitpid(pid2, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Child process 2 failed\n");
        kill_children(pid1, pid2, pid3);
        unlink(fifo1);
        unlink(fifo2);
        return 1;
    }

    waitpid(pid3, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Child process 3 failed\n");
        kill_children(pid1, pid2, pid3);
        unlink(fifo1);
        unlink(fifo2);
        return 1;
    }

    unlink(fifo1);
    unlink(fifo2);

    return 0;
}