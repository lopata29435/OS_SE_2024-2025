#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER_SIZE 128

void create_fifo(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

int process_file(const char *input_file, const char *fifo_name) {
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

    int fifo_fd = open(fifo_name, O_WRONLY);
    if (fifo_fd < 0) {
        perror("Error opening FIFO for writing");
        close(fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(fifo_fd, buffer, bytes_read) != bytes_read) {
            perror("Error writing to FIFO");
            close(fd);
            close(fifo_fd);
            return 1;
        }
    }

    close(fd);
    close(fifo_fd);
    return 0;
}

int process_output(const char *fifo_name, const char *output_file) {
    int fifo_fd = open(fifo_name, O_RDONLY);
    if (fifo_fd < 0) {
        perror("Error opening FIFO for reading");
        return 1;
    }

    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error opening output file");
        close(fifo_fd);
        return 1;
    }

    int counts[5] = {0};
    ssize_t bytes_read = read(fifo_fd, counts, sizeof(counts));
    if (bytes_read < 0) {
        perror("Error reading from FIFO");
        close(fd);
        close(fifo_fd);
        return 1;
    }

    const char *keywords[] = {"int", "return", "if", "else", "while"};
    char buffer[256];
    for (int i = 0; i < 5; i++) {
        int len = snprintf(buffer, sizeof(buffer), "%s: %d\n", keywords[i], counts[i]);
        write(fd, buffer, len);
    }

    close(fd);
    close(fifo_fd);
    return 0;
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

    if (process_file(argv[1], fifo1) != 0) {
        unlink(fifo1);
        unlink(fifo2);
        return 1;
    }

    if (process_output(fifo2, argv[2]) != 0) {
        unlink(fifo1);
        unlink(fifo2);
        return 1;
    }

    unlink(fifo1);
    unlink(fifo2);

    return 0;
}