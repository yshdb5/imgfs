#include "imgfs.h"
#include "util.h"
#include <stddef.h> // size_t
#include <stdint.h> // uint16_t
#include <sys/types.h> // ssize_t
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define ERR_NETWORK -1

int tcp_server_init(uint16_t port)
{
    int new_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (new_socket == -1) {
        perror("Error creating socket");
        return ERR_IO;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int enable = 1;
    if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error setting socket options");
        close(new_socket);
        return ERR_IO;
    }

    if (bind(new_socket, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        perror("Error during the bind");
        close(new_socket);
        return ERR_IO;
    }

    if (listen(new_socket, 15) == -1) {
        perror("Error lors de l'écoute");
        close(new_socket);
        return ERR_IO;
    }

    return new_socket;
}

int tcp_accept(int passive_socket)
{
    if (passive_socket < 0) return ERR_INVALID_ARGUMENT;
    else return accept(passive_socket, NULL, NULL);
}

ssize_t tcp_read(int active_socket, char* buffer, size_t buffer_len)
{
    M_REQUIRE_NON_NULL(buffer);
    if (buffer_len < 0) return ERR_INVALID_ARGUMENT;

    return recv(active_socket, buffer, buffer_len, 0);
}

ssize_t tcp_send(int active_socket, const char* response, size_t response_len)
{
    M_REQUIRE_NON_NULL(response);
    if (response_len <= 0) return ERR_INVALID_ARGUMENT;

    return send(active_socket, response, response_len, 0);
}
