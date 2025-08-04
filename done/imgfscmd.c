/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>

#define MAPPINGS_NUMBER 6

typedef int (*command)(int argc, char* argv[]);

typedef struct {
    const char name[MAX_IMGFS_NAME];
    command cmd;
} command_mapping;

command_mapping help_cmd = {"help", help};
command_mapping list_cmd = {"list", do_list_cmd};
command_mapping create_cmd = {"create", do_create_cmd};
command_mapping delete_cmd = {"delete", do_delete_cmd};
command_mapping insert_cmd = {"insert", do_insert_cmd};
command_mapping read_cmd = {"read", do_read_cmd};

command_mapping* commands[MAPPINGS_NUMBER] =
{&help_cmd, &list_cmd, &create_cmd, &delete_cmd, &insert_cmd, &read_cmd};


/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    VIPS_INIT(argv[0]);

    int ret = 0;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        ret = ERR_INVALID_COMMAND;

        argc--; argv++; // skip program name

        for (size_t i = 0; i < MAPPINGS_NUMBER; i++) {

            if (strcmp(argv[0], commands[i]->name) == 0) {
                argc--; argv++;
                ret = commands[i]->cmd(argc, argv);

                break;
            }
        }
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }

    vips_shutdown();

    return ret;
}
