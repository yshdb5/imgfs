#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include "util.h"
#include "socket_layer.h"

#define MAX_FILE_SIZE 1024
#define DELIMITER_SIZE "<EOF>"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    uint16_t port = atouint16(argv[1]);

    int server_socket = tcp_server_init(port);

    if (server_socket < 0) {
        fprintf(stderr, "ERROR while initialising the network\n");
        return 1;
    }

    printf("\nServer started on port %d\n", port);

    while (1) {

        printf("Waiting for a size...\n");

        int client_socket = tcp_accept(server_socket);
        if (client_socket < 0) {
            perror("ERROR, accepting client failed");
            continue;
        }

        char size_str[32];
        if (tcp_read(client_socket, size_str, sizeof(size_str)) < 0) {
            perror("ERROR, receiving size failed");
            close(client_socket);
            continue;
        }

        strtok(size_str, DELIMITER_SIZE);
        long file_size = atol(size_str);
        printf("Received a size: %ld --> accepted\n", file_size);

        char ack_response[32] = "Large file";
        if (file_size < MAX_FILE_SIZE) {
            strcpy(ack_response, "Small file");
            printf("About to receive file of %ld bytes\n", file_size);
        }

        if (tcp_send(client_socket, ack_response, strlen(ack_response)) < 0) {
            perror("ERROR, sending server final acknowledge failed");
            close(client_socket);
            continue;
        }

        if (file_size < MAX_FILE_SIZE) {
            char file_content[MAX_FILE_SIZE + 1] = {0};
            if (tcp_read(client_socket, file_content, file_size) < 0) {
                perror("ERROR, receiving file failed");
                close(client_socket);
                continue;
            }
            file_content[file_size] = '\0';
            printf("Received a file: \n%s\n", file_content);


            if (tcp_send(client_socket, "Accepted", strlen("Accepted\0")) < 0) {
                perror("ERROR, receiving server final acknowledge failed");
                close(client_socket);
                continue;
            }
        }

        close(client_socket);
    }
    return 0;
}
