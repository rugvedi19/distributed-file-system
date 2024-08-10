#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8085
#define BUFFER_SIZE 1024
#define SPDF_PORT 8081
#define STEXT_PORT 8082

void handle_client(int client_sock);
void handle_ufile(int client_sock, char *filename, char *dest_path);
void handle_dfile(int client_sock, char *filename);
void handle_rmfile(int client_sock, char *filename);
void handle_dtar(int client_sock, char *filetype);
void handle_display(int client_sock, char *pathname);
void connect_to_server(const char *ip, int port, int *sock);

int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 3) < 0)
    {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Smain server listening on port %d...\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) >= 0)
    {
        printf("New client connected to Smain\n");

        if (fork() == 0)
        {
            close(server_sock);
            handle_client(client_sock);
            close(client_sock);
            exit(0);
        }
        else
        {
            close(client_sock);
            wait(NULL);
        }
    }

    if (client_sock < 0)
    {
        perror("Accept failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    close(server_sock);
    return 0;
}

void handle_client(int client_sock)
{
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(command, 0, BUFFER_SIZE);
        memset(arg1, 0, BUFFER_SIZE);
        memset(arg2, 0, BUFFER_SIZE);

        // Receive data from client
        int recv_len = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (recv_len <= 0)
        {
            if (recv_len == 0)
            {
                printf("Client disconnected\n");
            }
            else
            {
                perror("recv failed");
            }
            break;
        }

        // Null-terminate the buffer
        buffer[recv_len] = '\0';

        // Parse command
        sscanf(buffer, "%s %s %s", command, arg1, arg2);

        printf("Received command: %s\n", command);
        printf("arg1: %s\n", arg1);
        printf("arg2: %s\n", arg2);

        // Handle command
        if (strcmp(command, "ufile") == 0)
        {
            handle_ufile(client_sock, arg1, arg2);
        }
        else if (strcmp(command, "dfile") == 0)
        {
            handle_dfile(client_sock, arg1);
        }
        else if (strcmp(command, "rmfile") == 0)
        {
            handle_rmfile(client_sock, arg1);
        }
        else if (strcmp(command, "dtar") == 0)
        {
            handle_dtar(client_sock, arg1);
        }
        else if (strcmp(command, "display") == 0)
        {
            handle_display(client_sock, arg1);
        }
        else
        {
            char *msg = "Invalid command\n";
            send(client_sock, msg, strlen(msg), 0);
        }
    }
}

int create_directory_recursive(const char *path) {
    char temp[256];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);

    if (temp[len - 1] == '/') {
        temp[len - 1] = '\0';
    }

    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    return 0;
}

void handle_ufile(int client_sock, char *filename, char *dest_path)
{
    char file_path[BUFFER_SIZE];
    char destination_path[BUFFER_SIZE];
    FILE *file;
    char response[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    int bytes_received;

    // Construct the file path on the server
    printf("Thinking 0...\n");
    snprintf(destination_path, sizeof(destination_path), "/home/garg83/smain/%s", dest_path);
    printf("Thinking...\n");
    // Create the destination directory if it doesn't exist
    if (create_directory_recursive(destination_path) != 0)
    {
        return ;
    }

    snprintf(file_path, sizeof(file_path), "/home/garg83/smain/%s/%s", dest_path, filename);
    // Open the file for writing
    file = fopen(file_path, "wb");
    if (file == NULL)
    {
        snprintf(response, sizeof(response), "Failed to open file %s for writing\n", file_path);
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Receive file data from the client
    printf("Receiving file: %s\n", file_path);
    while ((bytes_received = recv(client_sock, file_buffer, sizeof(file_buffer), 0)) > 0)
    {
        printf("Received %d bytes\n", bytes_received);
        fwrite(file_buffer, 1, bytes_received, file);
        if (bytes_received < sizeof(file_buffer))
        {
            printf("End of file detected\n");
            break; // End of file
        }
    }

    printf("Thinking 1...\n");
    snprintf(response, sizeof(response), "File %s uploaded successfully\n", filename);
    send(client_sock, response, strlen(response), 0);

    // Transfer .txt and .pdf files to the respective servers
    if (strstr(filename, ".txt") != NULL)
    {
        int stext_sock;
        connect_to_server("127.0.0.1", STEXT_PORT, &stext_sock);
        send(stext_sock, file_buffer, BUFFER_SIZE, 0);
        close(stext_sock);
    }
    else if (strstr(filename, ".pdf") != NULL)
    {
        int spdf_sock;
        connect_to_server("127.0.0.1", SPDF_PORT, &spdf_sock);
        send(spdf_sock, file_buffer, BUFFER_SIZE, 0);
        close(spdf_sock);
    }
}

void handle_dfile(int client_sock, char *filename)
{
    char file_path[BUFFER_SIZE];
    FILE *file;
    char buffer[BUFFER_SIZE];
    size_t file_size;

    snprintf(file_path, sizeof(file_path), "~/smain/%s", filename);

    file = fopen(file_path, "rb");
    if (!file)
    {
        perror("Failed to open file");
        return;
    }

    while ((file_size = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        send(client_sock, buffer, file_size, 0);
    }

    fclose(file);

    // If file is .txt or .pdf, fetch from Spdf or Stext server
    if (strstr(filename, ".txt") != NULL)
    {
        int stext_sock;
        connect_to_server("127.0.0.1", STEXT_PORT, &stext_sock);
        send(stext_sock, filename, strlen(filename), 0);
        while ((file_size = recv(stext_sock, buffer, BUFFER_SIZE, 0)) > 0)
        {
            send(client_sock, buffer, file_size, 0);
        }
        close(stext_sock);
    }
    else if (strstr(filename, ".pdf") != NULL)
    {
        int spdf_sock;
        connect_to_server("127.0.0.1", SPDF_PORT, &spdf_sock);
        send(spdf_sock, filename, strlen(filename), 0);
        while ((file_size = recv(spdf_sock, buffer, BUFFER_SIZE, 0)) > 0)
        {
            send(client_sock, buffer, file_size, 0);
        }
        close(spdf_sock);
    }
}

void handle_rmfile(int client_sock, char *filename)
{
    char file_path[BUFFER_SIZE];

    snprintf(file_path, sizeof(file_path), "~/smain/%s", filename);
    remove(file_path);

    // Request removal from Spdf or Stext if the file is .pdf or .txt
    if (strstr(filename, ".txt") != NULL)
    {
        int stext_sock;
        connect_to_server("127.0.0.1", STEXT_PORT, &stext_sock);
        send(stext_sock, filename, strlen(filename), 0);
        close(stext_sock);
    }
    else if (strstr(filename, ".pdf") != NULL)
    {
        int spdf_sock;
        connect_to_server("127.0.0.1", SPDF_PORT, &spdf_sock);
        send(spdf_sock, filename, strlen(filename), 0);
        close(spdf_sock);
    }
}

void handle_dtar(int client_sock, char *filetype)
{
    char tar_command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    FILE *tar_file;
    size_t file_size;

    if (strcmp(filetype, ".c") == 0)
    {
        snprintf(tar_command, sizeof(tar_command), "tar -cvf cfiles.tar ~/smain/*.c");
        system(tar_command);
        tar_file = fopen("cfiles.tar", "rb");
    }
    else if (strcmp(filetype, ".pdf") == 0)
    {
        snprintf(tar_command, sizeof(tar_command), "tar -cvf pdfs.tar ~/spdf/*.pdf");
        system(tar_command);
        tar_file = fopen("pdfs.tar", "rb");
    }
    else if (strcmp(filetype, ".txt") == 0)
    {
        snprintf(tar_command, sizeof(tar_command), "tar -cvf txts.tar ~/stext/*.txt");
        system(tar_command);
        tar_file = fopen("txts.tar", "rb");
    }
    else
    {
        char *msg = "Unsupported file type\n";
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    if (!tar_file)
    {
        perror("Failed to open tar file");
        return;
    }

    while ((file_size = fread(buffer, 1, BUFFER_SIZE, tar_file)) > 0)
    {
        send(client_sock, buffer, file_size, 0);
    }

    fclose(tar_file);
}

void handle_display(int client_sock, char *pathname)
{
    char command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    FILE *pipe;
    size_t file_size;

    snprintf(command, sizeof(command), "find ~/smain/%s -type f \( -name '*.c' -o -name '*.pdf' -o -name '*.txt' \)", pathname);
    pipe = popen(command, "r");
    if (!pipe)
    {
        perror("popen failed");
        return;
    }

    while ((file_size = fread(buffer, 1, BUFFER_SIZE, pipe)) > 0)
    {
        send(client_sock, buffer, file_size, 0);
    }

    pclose(pipe);
}

void connect_to_server(const char *ip, int port, int *sock)
{
    struct sockaddr_in server_addr;

    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(*sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
}
