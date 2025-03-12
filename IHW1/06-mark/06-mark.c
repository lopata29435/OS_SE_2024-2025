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

int process_file(const char *input_file, int pipe_out) {
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
    ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("Error reading input file");
        close(fd);
        return 1;
    }
    buffer[bytes_read] = '\0';

    write(pipe_out, buffer, bytes_read + 1);
    close(fd);
    return 0;
}

int process_text(int pipe_in, int pipe_out) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(pipe_in, buffer, BUFFER_SIZE);
    if (bytes_read < 0) {
        perror("Error reading from pipe");
        return 1;
    }

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

    write(pipe_out, counts, sizeof(counts));
    return 0;
}

int process_output(int pipe_in, const char *output_file) {
    int counts[KEYWORDS_COUNT];
    ssize_t bytes_read = read(pipe_in, counts, sizeof(counts));
    if (bytes_read < 0) {
        perror("Error reading from pipe");
        return 1;
    }

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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    int pipe1[2], pipe2[2];
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);

        if (process_text(pipe1[0], pipe2[1]) != 0) {
            exit(EXIT_FAILURE);
        }

        close(pipe1[0]);
        close(pipe2[1]);
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    close(pipe1[0]);
    close(pipe2[1]);

    if (process_file(argv[1], pipe1[1]) != 0) {
        kill(pid, SIGTERM);
        close(pipe1[1]);
        close(pipe2[0]);
        return 1;
    }
    close(pipe1[1]);

    if (process_output(pipe2[0], argv[2]) != 0) {
        kill(pid, SIGTERM);
        close(pipe2[0]);
        return 1;
    }
    close(pipe2[0]);

    waitpid(pid, NULL, 0);
    return 0;
}