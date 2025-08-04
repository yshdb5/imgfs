/*
 * @file imgfs_server_services.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t
#include <pthread.h>
#include <vips/vips.h>

#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"
#include "http_prot.h"


#define MAX_CHARACTERE_RES 5

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port;

// Global variables
pthread_mutex_t mutex;

#define URI_ROOT "/imgfs"

/********************************************************************//**
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionnaly port number as argv[2]
 ********************************************************************** */
int server_startup (int argc, char **argv)
{
    VIPS_INIT(argv[0]);

    if (argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS;

    pthread_mutex_init(&mutex, NULL);

    M_REQUIRE_NON_NULL(argv[1]);

    memset(&fs_file, 0, sizeof(struct imgfs_file));

    int err = do_open(argv[1], "rb+", &fs_file);

    if (err != ERR_NONE) return err;

    print_header(&fs_file.header);

    if (argc > 2) {
        M_REQUIRE_NON_NULL(argv[2]);
        uint16_t potential_port = atouint16(argv[2]);
        server_port = (potential_port == 0) ? DEFAULT_LISTENING_PORT : potential_port;
    } else {
        server_port = DEFAULT_LISTENING_PORT;
    }

    err = http_init(server_port, handle_http_message);

    fprintf(stderr, "ImgFS server started on http://localhost:%d\n", server_port);

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        do_close(&fs_file);
        return ERR_RUNTIME;
    }

    return ERR_NONE;
}

/********************************************************************//**
 * Shutdown function. Free the structures and close the file.
 ********************************************************************** */
void server_shutdown (void)
{
    fprintf(stderr, "Shutting down...\n");
    http_close();
    do_close(&fs_file);

    vips_shutdown();

    if (pthread_mutex_destroy(&mutex) != 0) {
        fprintf(stderr, "Error destroying mutex\n");
    }
}

/**********************************************************************
 * Sends error message.
 ********************************************************************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**********************************************************************
 * Sends 302 OK message.
 ********************************************************************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

static int handle_list_call(int connection)
{
    char *json_body;
    pthread_mutex_lock(&mutex);
    int err = do_list(&fs_file, JSON, &json_body);
    pthread_mutex_unlock(&mutex);
    if (err != ERR_NONE) {
        return err;
    }
    return http_reply(connection, "200 OK", "Content-Type: application/json", json_body, strlen(json_body));
}

static int handle_read_call(int connection, struct http_message* msg)
{
    char res[MAX_CHARACTERE_RES + 1] = {0};
    char img_id[MAX_IMG_ID] = {0};
    //TODO: how can we know the size of the res in advance?
    int err = http_get_var(&msg->uri, "res", res, sizeof(res));

    if (err <= 0) {
        return reply_error_msg(connection, ERR_INVALID_ARGUMENT);
    }

    err = http_get_var(&msg->uri, "img_id", img_id, sizeof(img_id));

    if (err <= 0) {
        return reply_error_msg(connection, ERR_INVALID_ARGUMENT);
    }

    int resolution = resolution_atoi(res);

    if (resolution == -1) {
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
    }

    char* image_data;
    uint32_t image_size;
    err = do_read(img_id, resolution, &image_data, &image_size, &fs_file);
    if (err != ERR_NONE) {
        return reply_error_msg(connection, err);
    }

    return http_reply(connection, "200 OK", "Content-Type: image/jpeg",
                      image_data, image_size);

}

static int handle_delete_call(int connection, struct http_message* msg)
{
    char *img_id;
    //TODO: how can we know the size of the img_id in advance?
    int err = http_get_var(&msg->uri, "img_id", img_id, sizeof(img_id));

    if (err <= 0) {
        return reply_error_msg(connection, ERR_INVALID_ARGUMENT);
    }

    err = do_delete(img_id, &fs_file);

    if (err != ERR_NONE) {
        return reply_error_msg(connection, err);
    }

    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/index.html" HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

static int handle_insert_call(int connection, struct http_message* msg)
{
    char img_name[MAX_IMGFS_NAME] = {0};
    char *img_data;
    size_t img_size;

    int err = http_get_var(&msg->uri, "name", img_name, sizeof(img_name));
    if (err <= 0) {
        return reply_error_msg(connection, ERR_INVALID_ARGUMENT);
    }

    //err = http_parse_message(msg->body, &img_data, &img_size); ?? AHHHHHHHHHHHHHHHHHHHHHHHHH
    if (err != ERR_NONE) {
        return reply_error_msg(connection, err);
    }

    err = do_insert(img_data, img_size, img_name, &fs_file);
    if (err != ERR_NONE) {
        return reply_error_msg(connection, err);
    }

    return reply_302_msg(connection);
}

/**********************************************************************
 * Simple handling of http message. TO BE UPDATED WEEK 13
 ********************************************************************** */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);

    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(connection, BASE_FILE);
    }

    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",
                 connection,
                 (int) msg->uri.len, msg->uri.val);

    if (http_match_uri(msg, URI_ROOT "/list")) {
        pthread_mutex_lock(&mutex);
        handle_list_call(connection);
        pthread_mutex_unlock(&mutex);
    } else if (http_match_uri(msg, URI_ROOT "/read")) {
        pthread_mutex_lock(&mutex);
        handle_read_call(connection, msg);
        pthread_mutex_unlock(&mutex);
    } else if (http_match_uri(msg, URI_ROOT "/delete")) {
        pthread_mutex_lock(&mutex);
        handle_delete_call(connection, msg);
        pthread_mutex_unlock(&mutex);
    } else if (http_match_uri(msg, URI_ROOT "/insert")
               && http_match_verb(&msg->method, "POST")) {
        pthread_mutex_lock(&mutex);
        handle_insert_call(connection, msg);
        pthread_mutex_unlock(&mutex);
    } else {
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
    }
}
