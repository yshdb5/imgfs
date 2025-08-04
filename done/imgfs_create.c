#include "imgfs.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*******************************************************************
 * Creates the imgFS called imgfs_filename. Writes the header and the
 * preallocated empty metadata array to imgFS file.
 */
int do_create(const char* imgfs_filename, struct imgfs_file* imgfs_file)
{

    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(imgfs_file);

    strncpy(imgfs_file->header.name, CAT_TXT, MAX_IMGFS_NAME);
    imgfs_file->header.version = 0;
    imgfs_file->header.nb_files = 0;

    size_t max_files = imgfs_file->header.max_files;

    imgfs_file->metadata = calloc(max_files, sizeof(struct img_metadata));

    if (imgfs_file->metadata == NULL) return ERR_OUT_OF_MEMORY;

    FILE* file = fopen(imgfs_filename, "wb");

    if (file == NULL) return ERR_IO;

    if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, file) != 1) {
        return ERR_IO;
    }

    if (fwrite(imgfs_file->metadata, sizeof(struct img_metadata), max_files, file)
        != max_files) {
        return ERR_IO;
    }

    imgfs_file->file = file;

    printf("%d item(s) written\n", imgfs_file->header.max_files + 1);

    return ERR_NONE;
}
