/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"
#include "http_prot.h"
#include <pthread.h>

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

#define M_REQUIRE_NON_NULL(arg) if ((arg) == NULL) return ERR_INVALID_ARGUMENT;
#define ERR_INVALID_ARGUMENT -1
#define ERR_OUT_OF_MEMORY -2
#define ERR_IO -3

#define HTTP_PROTOCOL_ID "HTTP/1.1 "
#define HTTP_LINE_DELIM "\r\n"
#define HTTP_HDR_END_DELIM "\r\n\r\n"

/*******************************************************************
 * Handle connection
 */
static void *handle_connection(void *arg)
{
    M_REQUIRE_NON_NULL(arg);
    sigset_t mask;
    size_t total_read = 0;
    ssize_t read = 0;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    int *active_socket = (int*) arg;

    if (active_socket == NULL) {
        perror("active_socket in handle_connection() is NULL");
        return &our_ERR_INVALID_ARGUMENT;
    }

    char *buf = calloc(MAX_HEADER_SIZE, sizeof(char));

    if (buf == NULL) {
        perror("calloc() in handle_connection()");
        close(*active_socket);
        free(active_socket);
        return &our_ERR_OUT_OF_MEMORY;
    }

    while(total_read < MAX_HEADER_SIZE && strstr(buf, HTTP_HDR_END_DELIM) == NULL) {
        read = tcp_read(*active_socket, buf + total_read, MAX_HEADER_SIZE - total_read);
        if (read < 0) {
            perror("tcp_read() in handle_connection()");
            close(*active_socket);
            free(active_socket);
            free(buf);
            return &our_ERR_IO;
        }

        total_read += read;
    }

    if (read <= 0) {
        perror("tcp_read1() in handle_connection()");
        close(*active_socket);
        free(active_socket);
        free(buf);
        return &our_ERR_IO;
    }


    while (strstr(buf, HTTP_HDR_END_DELIM) == NULL && read > 0) {

        read = tcp_read(*active_socket, buf, MAX_HEADER_SIZE);

        if (read <= 0) {
            perror("tcp_read2() in handle_connection()");
            close(*active_socket);
            free(active_socket);
            free(buf);
            return &our_ERR_IO;
        }

        int err = ERR_NONE;

        struct http_message msg;
        int content_len = 0;

        err = http_parse_message(buf, read, &msg, &content_len);

        if (err < 0) {
            free(buf);
            close(*active_socket);
            free(active_socket);
            return &err;
        }

        if (read == 0 && content_len > 0 && msg.body.len < content_len) {
            char* extended_buf = realloc(buf, MAX_HEADER_SIZE + content_len);
            if (extended_buf == NULL) {
                perror("realloc() in handle_connection()");
                free(buf);
                close(*active_socket);
                free(active_socket);
                return &our_ERR_OUT_OF_MEMORY;
            }

            buf = extended_buf;
            ssize_t extended_read = tcp_read(*active_socket, buf + read,
                                             MAX_HEADER_SIZE + content_len - read);
            if (extended_read == -1) {
                perror("tcp_read3() in handle_connection()");
                free(buf);
                close(*active_socket);
                free(active_socket);
                return &our_ERR_IO;
            }

            read += extended_read;
        }

        if (read > 0) {
            ssize_t remaining_read = tcp_read(*active_socket, buf + read,
                                              MAX_HEADER_SIZE + content_len - read);
            if (remaining_read > 0) {
                read += remaining_read;
                err = cb(&msg, *active_socket);
                content_len = 0;
            }
        }

        if (err != ERR_NONE) {
            free(buf);
            close(*active_socket);
            free(active_socket);
            return &err;
        }

        if (read == 0) {
            free(buf);
            close(*active_socket);
            free(active_socket);
            return &our_ERR_NONE;
        }

    }

    close(*active_socket);
    free(active_socket);
    free(buf);
    return &our_ERR_NONE;
}


/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    cb = callback;
    passive_socket = tcp_server_init(port);
    return passive_socket;
}

/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("close() in http_close()");
        else
            passive_socket = -1;
    }
}

/*******************************************************************
 * Receive content
 */
int http_receive(void)
{

    int active_socket = tcp_accept(passive_socket);
    if (active_socket < 0) {
        perror("tcp_accept() in http_receive()");
        return ERR_IO;
    }

    int *socket = malloc(sizeof(int));
    if (socket == NULL) {
        perror("calloc() in http_receive()");
        close(active_socket);
        return ERR_OUT_OF_MEMORY;
    }

    *socket = active_socket;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t thread;
    int ret = pthread_create(&thread, &attr, handle_connection, (void*) socket);

    if (ret != 0) {
        perror("pthread_create() in http_receive()");
        close(active_socket);
        free(socket);
        return ERR_IO;
    }

    pthread_attr_destroy(&attr);

    return ERR_NONE;
}

/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}

/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers,
               const char *body, size_t body_len)
{
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);

    size_t header_length = strlen(HTTP_PROTOCOL_ID) + strlen(status)
                           + strlen(HTTP_LINE_DELIM) + strlen(headers) + strlen("Content-Length: ")
                           + snprintf(NULL, 0, "%zu", body_len) + strlen(HTTP_HDR_END_DELIM) + 1;

    char* buf = calloc(header_length, sizeof(char));

    if (buf == NULL) return ERR_OUT_OF_MEMORY;

    sprintf(buf, "%s%s%s%s%s%zu%s",
            HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM, headers, "Content-Length: ",
            body_len, HTTP_HDR_END_DELIM);

    if (body_len > 0) {
        M_REQUIRE_NON_NULL(body);
        buf = realloc(buf, header_length + body_len);
        if (buf == NULL) return ERR_OUT_OF_MEMORY;
        strcat(buf, body);
    }

    if (tcp_send(connection, buf, strlen(buf)) == -1) return ERR_IO;

    return ERR_NONE;
}
