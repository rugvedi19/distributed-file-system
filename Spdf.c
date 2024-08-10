#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8081
#define BUFFER_SIZE 1024

void handle_client(int client_sock);
void handle_ufile(int client_sock, char *filename, char *dest_path);
void handle_dfile(int client_sock, char *filename);
void handle_rmfile(int client_sock, char *filename);
void handle_dtar(int client_sock, char *filetype);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 3) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Spdf server listening on port %d...\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) >= 0) {
        printf("New client connected to Spdf\n");

        if (fork() == 0) {
            close(server_sock);
            handle_client(client_sock);
            close(client_sock);
            exit(0);
        } else {
            close(client_sock);
            wait(NULL);
        }
    }

    if (client_sock < 0) {
        perror("Accept failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    close(server_sock);
    return 0;
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(command, 0, BUFFER_SIZE);
        memset(arg1, 0, BUFFER_SIZE);
        memset(arg2, 0, BUFFER_SIZE);

        if (recv(client_sock, buffer, BUFFER_SIZE, 0) <= 0) {
            perror("recv failed");
            break;
        }

        sscanf(buffer, "%s %s %s", command, arg1, arg2);

        if (strcmp(command, "ufile") == 0) {
            handle_ufile(client_sock, arg1, arg2);
        } else if (strcmp(command, "dfile") == 0) {
            handle_dfile(client_sock, arg1);
        } else if (strcmp(command, "rmfile") == 0) {
            handle_rmfile(client_sock, arg1);
        } else if (strcmp(command, "dtar") == 0) {
            handle_dtar(client_sock, arg1);
        } else {
            char *msg = "Invalid command\n";
            send(client_sock, msg, strlen(msg), 0);
        }
    }
}

void handle_ufile(int client_sock, char *filename, char *dest_path) {
    char file_path[BUFFER_SIZE];
    FILE *file;
    char buffer[BUFFER_SIZE];
    size_t file_size;

    snprintf(file_path, sizeof(file_path), "~/spdf/%s", dest_path);
    struct stat st = {0};
    if (stat(file_path, &st) == -1) {
        mkdir(file_path, 0700);
    }

    file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    while ((file_size = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, file_size, file);
        if (file_size < BUFFER_SIZE) break;
    }

    fclose(file);
}

void handle_dfile(int client_sock, char *filename) {
    char file_path[BUFFER_SIZE];
    FILE *file;
    char buffer[BUFFER_SIZE];
    size_t file_size;

    snprintf(file_path, sizeof(file_path), "~/spdf/%s", filename);

    file = fopen(file_path, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    while ((file_size = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_sock, buffer, file_size, 0);
    }

    fclose(file);
}

void handle_rmfile(int client_sock, char *filename) {
    char file_path[BUFFER_SIZE];

    snprintf(file_path, sizeof(file_path), "~/spdf/%s", filename);
    remove(file_path);
}

void handle_dtar(int client_sock, char *filetype) {
    char tar_command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    FILE *tar_file;
    size_t file_size;

    if (strcmp(filetype, ".pdf") == 0) {
        snprintf(tar_command, sizeof(tar_command), "tar -cvf pdfs.tar ~/spdf/*.pdf");
        system(tar_command);
        tar_file = fopen("pdfs.tar", "rb");
    } else {
        char *msg = "Unsupported file type\n";
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    if (!tar_file) {
        perror("Failed to open tar file");
        return;
    }

    while ((file_size = fread(buffer, 1, BUFFER_SIZE, tar_file)) > 0) {
        send(client_sock, buffer, file_size, 0);
    }

    fclose(tar_file);
}
