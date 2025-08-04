#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include "util.h"
#include "socket_layer.h"

#define DELIMITER_SIZE "<EOF>"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <file_path>\n", argv[0]);
        return 1;
    }

    uint16_t port = atouint16(argv[1]);
    const char *file_path = argv[2];


    struct stat file_state;
    if (stat(file_path, &file_state) != 0 || file_state.st_size >= 2048) {
        fprintf(stderr, "ERROR, the file is too big or doesn't exist.\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    printf("Talking to %d\n", port);
    char size_msg[file_state.st_size];
    sprintf(size_msg, "%ld%s", file_state.st_size, DELIMITER_SIZE);

    if (tcp_send(sock, size_msg, strlen(size_msg)) < 0) {
        perror("ERROR, sending file size failed");
        close(sock);
        return 1;
    }

    printf("Sending size: %ld\n", file_state.st_size);

    char server_response[32] = {0} ;
    if (tcp_read(sock, server_response, sizeof(server_response)) < 0) {
        perror("ERROR, receiving server response failed");
        close(sock);
        return 1;
    }
    printf("Server responded: \"%s\"\n", server_response);

    if (strcmp(server_response, "Small file") == 0) {
        FILE *file = fopen(file_path, "rb");
        char buffer[1024];
        size_t bytes_read;
        printf("Sending %s:\n", file_path);
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {


            if (tcp_send(sock, buffer, bytes_read) < 0) {
                perror("ERROR, sending file failed");
                fclose(file);
                close(sock);
                return 1;
            }
        }
        fclose(file);
        int read_bytes = tcp_read(sock, server_response, sizeof(server_response));
        if (read_bytes < 0) {
            perror("ERROR, receiving server final acknowledge failed");
            close(sock);
            return 1;
        }
        server_response[read_bytes] = '\0';
        printf("%s\n", server_response);
    }

    printf("Done\n");
    close(sock);
    return 0;
}
