#include "imgfs.h"
#include "util.h"
#include "image_content.h"
#include "image_dedup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int do_read(const char *img_id, int resolution, char **image_buffer,
            uint32_t *image_size, struct imgfs_file *imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgfs_file);

    uint32_t i = 0;
    while(i < imgfs_file->header.max_files
          && strcmp(img_id, imgfs_file->metadata[i].img_id) != 0) i++;

    if (imgfs_file->header.nb_files == 0
        || i == imgfs_file->header.max_files
        || imgfs_file->metadata[i].is_valid == EMPTY) return ERR_IMAGE_NOT_FOUND;

    if (resolution < THUMB_RES || resolution > ORIG_RES) return ERR_RESOLUTIONS;

    if ((resolution == THUMB_RES || resolution == SMALL_RES) &&
        (imgfs_file->metadata[i].offset[resolution] == 0)) {
        int err = lazily_resize(resolution, imgfs_file, i);
        if (err != ERR_NONE) return err;
    }

    if (fseek(imgfs_file->file, (long) imgfs_file->metadata[i].offset[resolution], SEEK_SET) != 0) {
        return ERR_IO;
    }

    *image_buffer = calloc(1, imgfs_file->metadata[i].size[resolution]);

    if (*image_buffer == NULL) return ERR_OUT_OF_MEMORY;

    if (fread(*image_buffer, imgfs_file->metadata[i].size[resolution], 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    *image_size = imgfs_file->metadata[i].size[resolution];

    return ERR_NONE;
}
