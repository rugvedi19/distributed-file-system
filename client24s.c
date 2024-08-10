#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 8085
#define BUFFER_SIZE 1024

void send_command(int sock, const char *command, const char *arg1, const char *arg2)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s %s %s", command, arg1, arg2);
    if (send(sock, buffer, strlen(buffer), 0) == -1)
    {
        perror("send_command failed");
    }
}

void send_file(int sock, const char *filename)
{
    char buffer[BUFFER_SIZE];
    int file = open(filename, O_RDONLY);
    if (file < 0)
    {
        perror("open file");
        return;
    }

    // Check the size of the file
    off_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET); // Rewind to the start of the file

    if (file_size == 0)
    {
        printf("File is empty: %s\n", filename);
        send(sock, "", 1, 0);
    }
    else
    {
        ssize_t bytes_read;
        while ((bytes_read = read(file, buffer, sizeof(buffer))) > 0)
        {
            if (send(sock, buffer, bytes_read, 0) == -1)
            {
                perror("send file");
                break;
            }
        }
    }

    close(file);
}

void handle_command(int sock, const char *command, const char *arg1, const char *arg2)
{
    if (strcmp(command, "ufile") == 0)
    {
        send_command(sock, command, arg1, arg2);
        send_file(sock, arg1);
    }
    else if (strcmp(command, "dfile") == 0 || strcmp(command, "rmfile") == 0 || strcmp(command, "dtar") == 0 || strcmp(command, "display") == 0)
    {
        send_command(sock, command, arg1, arg2);
    }
    else
    {
        printf("Unknown command: %s\n", command);
    }
}

int main()
{
    int sock;
    struct sockaddr_in server_addr;
    char command[BUFFER_SIZE], arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];
    char *token;
    char input[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Get command from user
        printf("Enter command: ");
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            perror("fgets failed");
            continue;
        }

        // Remove newline character
        input[strcspn(input, "\n")] = 0;

        // Tokenize the input
        token = strtok(input, " ");
        if (token == NULL)
        {
            printf("No command entered.\n");
            continue;
        }

        strncpy(command, token, sizeof(command));
        token = strtok(NULL, " ");
        if (token != NULL)
        {
            strncpy(arg1, token, sizeof(arg1));
            token = strtok(NULL, " ");
            if (token != NULL)
            {
                strncpy(arg2, token, sizeof(arg2));
            }
            else
            {
                arg2[0] = '\0';
            }
        }
        else
        {
            arg1[0] = '\0';
            arg2[0] = '\0';
        }

        // Send the command
        handle_command(sock, command, arg1, arg2);

        // Receive and display response (optional)
        char response[BUFFER_SIZE];
        int len = recv(sock, response, sizeof(response) - 1, 0);
        if (len > 0)
        {
            response[len] = '\0'; // Null-terminate the received data
            printf("Server response: %s\n", response);
        }
        else if (len == 0)
        {
            printf("Server disconnected.\n");
            break;
        }
        else
        {
            perror("recv failed");
        }
    }

    close(sock);
    return 0;
}
