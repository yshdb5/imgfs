/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;

// number of arguments
static const uint16_t ARGS_NBR_MAX_FILES = 2;
static const uint16_t ARGS_NBR_DO_DELETE = 2;
static const uint16_t ARGS_NBR_THUMB_SMALL_RES = 3;

static void create_name(const char* img_id, int resolution, char** new_name)
{
    const char* resolution_suffix = NULL;

    switch (resolution) {
    case ORIG_RES:
        resolution_suffix = "_orig";
        break;
    case THUMB_RES:
        resolution_suffix = "_thumbnail";
        break;
    case SMALL_RES:
        resolution_suffix = "_small";
        break;
    default:
        resolution_suffix = "_orig";
        break;
    }

    const char *suffix = ".jpg";

    *new_name = calloc(strlen(img_id) + strlen(resolution_suffix) + strlen(suffix) + 1, sizeof(char));

    if (*new_name != NULL) {
        strcpy(*new_name, img_id);
        strcat(*new_name, resolution_suffix);
        strcat(*new_name, suffix);
    }
}

static int write_disk_image(const char *filename, const char *image_buffer,
                            uint32_t image_size)
{
    FILE *file = fopen(filename, "wb");

    if (file == NULL) return ERR_IO;

    if (fwrite(image_buffer, image_size, 1, file) != 1) {
        fclose(file);
        return ERR_IO;
    }

    fclose(file);

    return ERR_NONE;
}

static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL) return ERR_IO;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ERR_IO;
    }

    long size = ftell(file);

    if (size < 0) {
        fclose(file);
        return ERR_IO;
    }

    *image_size = (uint32_t) size;

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ERR_IO;
    }

    *image_buffer = calloc(1, *image_size);

    if (*image_buffer == NULL) {
        fclose(file);
        return ERR_OUT_OF_MEMORY;
    }

    if (fread(*image_buffer, *image_size, 1, file) != 1) {
        free(*image_buffer);
        fclose(file);
        return ERR_IO;
    }

    fclose(file);

    return ERR_NONE;
}

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char** useless_too _unused)
{
    printf(
    "imgfscmd [COMMAND] [ARGUMENTS]\n"
    "   help: displays this help.\n"
    "   list <imgFS_filename>: list imgFS content.\n"
    "   create <imgFS_filename> [options]: create a new imgFS.\n"
    "       options are:\n"
    "           -max_files <MAX_FILES>: maximum number of files.\n"
    "                                   default value is 128\n"
    "                                   maximum value is 4294967295\n"
    "           -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
    "                                   default value is 64x64\n"
    "                                   maximum value is 128x128\n"
    "           -small_res <X_RES> <Y_RES>: resolution for small images.\n"
    "                                   default value is 256x256\n"
    "                                   maximum value is 512x512\n"
    "   read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
    "       read an image from the imgFS and save it to a file.\n"
    "       default resolution is \"original\".\n"
    "   insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n"
    "   delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n");
    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    if (argc != 1) return (argc < 1) ? ERR_INVALID_ARGUMENT : ERR_INVALID_COMMAND;

    struct imgfs_file imgfs_file = {0};

    M_REQUIRE_NON_NULL(argv[0]);

    int err = do_open(argv[0], "rb", &imgfs_file);

    if (err != ERR_NONE) {
        do_close(&imgfs_file);
        return err;
    }

    err = do_list(&imgfs_file, STDOUT, NULL);

    do_close(&imgfs_file);

    return err;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    puts("Create");

    if(argv == NULL) return ERR_INVALID_ARGUMENT;

    if (argv[0] == NULL) return ERR_NOT_ENOUGH_ARGUMENTS;

    uint32_t max_files = default_max_files;
    uint16_t thumb_resX = default_thumb_res;
    uint16_t thumb_resY = default_thumb_res;
    uint16_t small_resX = default_small_res;
    uint16_t small_resY = default_small_res;

    const char* imgfs_filename = argv[0];
    argc--; argv++;

    while(argc > 0) {
        if (strcmp(argv[0], "-max_files") == 0) {
            if (argc < ARGS_NBR_MAX_FILES) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            max_files = atouint32(argv[1]);
            if (max_files == 0 || max_files > UINT32_MAX) {
                return ERR_MAX_FILES;
            }
            argc -= ARGS_NBR_MAX_FILES;
            argv += ARGS_NBR_MAX_FILES;
        } else if (strcmp(argv[0], "-thumb_res") == 0) {
            if (argc < ARGS_NBR_THUMB_SMALL_RES) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            thumb_resX = atouint16(argv[1]);
            thumb_resY = atouint16(argv[2]);
            if (thumb_resX == 0 || thumb_resY == 0
                || thumb_resX > MAX_THUMB_RES || thumb_resY > MAX_THUMB_RES) {
                return ERR_RESOLUTIONS;
            }
            argc -= ARGS_NBR_THUMB_SMALL_RES;
            argv += ARGS_NBR_THUMB_SMALL_RES;
        } else if (strcmp(argv[0], "-small_res") == 0) {
            if (argc < ARGS_NBR_THUMB_SMALL_RES) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            small_resX = atouint16(argv[1]);
            small_resY = atouint16(argv[2]);
            if (small_resX == 0 || small_resY == 0
                || small_resX > MAX_SMALL_RES || small_resY > MAX_SMALL_RES) {
                return ERR_RESOLUTIONS;
            }
            argc -= ARGS_NBR_THUMB_SMALL_RES;
            argv += ARGS_NBR_THUMB_SMALL_RES;
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }

    struct imgfs_header header = {0};
    header.version = 0;
    header.max_files = max_files;
    header.resized_res[0] = thumb_resX;
    header.resized_res[1] = thumb_resY;
    header.resized_res[2] = small_resX;
    header.resized_res[3] = small_resY;

    struct imgfs_file imgfs_file = {0};

    imgfs_file.header = header;

    int err = do_create(imgfs_filename, &imgfs_file);

    do_close(&imgfs_file);

    return err;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    if (argv == NULL) return ERR_INVALID_ARGUMENT;

    if (argc < ARGS_NBR_DO_DELETE) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char* imgfs_filename = argv[0];
    const char* imgID = argv[1];

    if (strlen(imgID) == 0 || strlen(imgID) > MAX_IMG_ID) return ERR_INVALID_IMGID;

    struct imgfs_file imgfs_file = {0};

    int err = do_open(imgfs_filename, "rb+", &imgfs_file);

    if (err != ERR_NONE) {
        do_close(&imgfs_file);
        return err;
    }

    err = do_delete(imgID, &imgfs_file);

    do_close(&imgfs_file);

    return err;
}

int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}


int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}
