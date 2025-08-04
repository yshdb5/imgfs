#include "imgfs.h"
#include "util.h"
#include "image_content.h"
#include "image_dedup.h"
#include <stdio.h>
#include <string.h>

int do_insert(const char *image_buffer, size_t image_size,
              const char *img_id, struct imgfs_file *imgfs_file)
{
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);

    if (imgfs_file->header.nb_files >= imgfs_file->header.max_files) return ERR_IMGFS_FULL;

    uint32_t i = 0;
    while(imgfs_file->metadata[i].is_valid == NON_EMPTY) i++;


    imgfs_file->metadata[i].offset[THUMB_RES] = 0;
    imgfs_file->metadata[i].offset[SMALL_RES] = 0;
    imgfs_file->metadata[i].offset[ORIG_RES] = 0;

    imgfs_file->metadata[i].size[THUMB_RES] = 0;
    imgfs_file->metadata[i].size[SMALL_RES] = 0;
    imgfs_file->metadata[i].size[ORIG_RES] = 0;

    SHA256((const unsigned char*) image_buffer, image_size, imgfs_file->metadata[i].SHA);

    strcpy(imgfs_file->metadata[i].img_id, img_id);

    imgfs_file->metadata[i].size[ORIG_RES] = (uint32_t) image_size;

    uint32_t height = 0;
    uint32_t width = 0;

    int err = get_resolution(&height, &width, image_buffer, image_size);

    if (err != ERR_NONE) return err;

    imgfs_file->metadata[i].orig_res[0] = width;
    imgfs_file->metadata[i].orig_res[1] = height;

    imgfs_file->metadata[i].is_valid = NON_EMPTY;

    err = do_name_and_content_dedup(imgfs_file, i);

    if (err != ERR_NONE) return err;

    if (imgfs_file->metadata[i].offset[ORIG_RES] == 0) {

        if (fseek(imgfs_file->file, 0, SEEK_END) != 0) return ERR_IO;

        imgfs_file->metadata[i].offset[ORIG_RES] = (uint64_t) ftell(imgfs_file->file);

        if (fwrite(image_buffer, image_size, 1, imgfs_file->file) != 1) {
            return ERR_IO;
        }
    }

    imgfs_file->header.nb_files++;
    imgfs_file->header.version++;

    if (fseek(imgfs_file->file, 0, SEEK_SET) != 0) return ERR_IO;

    if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    if (fseek(imgfs_file->file, (long) (sizeof(struct imgfs_header) +
                                        i * sizeof(struct img_metadata)), SEEK_SET) != ERR_NONE) {
        return ERR_IO;
    }

    if (fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}
