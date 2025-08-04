/*
 * @file imgfs_server.c
 * @brief ImgFS server part, main
 *
 * @author Konstantinos Prasopoulos
 */

#include "util.h"
#include "imgfs.h"
#include "socket_layer.h"
#include "http_net.h"
#include "imgfs_server_service.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> // abort()

/********************************************************************/
static void signal_handler(int sig_num _unused)
{
    server_shutdown();
    exit(EXIT_SUCCESS);
}

/********************************************************************/
static void set_signal_handler(void)
{
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset() in set_signal_handler()");
        abort();
    }
    action.sa_handler = signal_handler;
    action.sa_flags   = 0;
    if ((sigaction(SIGINT,  &action, NULL) < 0) ||
        (sigaction(SIGTERM,  &action, NULL) < 0)) {
        perror("sigaction() in set_signal_handler()");
        abort();
    }
}

/************************
 * Comment débug : faire "gdb ./imgfs_server"
 * puis layout src
 * puis b server_startup
 * puis run /home/boren/Desktop/myfiles/Compsys/test/provided/tests/data/test02.imgfs 8000
 * tout va bien jusque quand on arrive à http_receive() et ça plante
 * dans tcp_accept(passive_socket) (première ligne).
 * 
 * gdb --args ./imgfs_server /home/dinee/Desktop/myfiles/CompSys/cs202-24-prj-8200/done/tests/data/empty2.imgfs 8000
 * layout src
 * b server_startup
 * run
 * 
 * ********************************************/

int main (int argc, char *argv[])
{
    set_signal_handler();

    int err = server_startup(argc, argv);

    if (err != ERR_NONE) {
        fprintf(stderr, "server_startup() failed\n");
        fprintf(stderr, "%s\n", ERR_MSG(err));
        return err;
    }

    while ((err = http_receive()) == ERR_NONE);

    fprintf(stderr, "http_receive() failed\n");
    fprintf(stderr, "%s\n", ERR_MSG(err));

    signal_handler(SIGQUIT);

    return err;
}
