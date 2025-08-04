#include "imgfs.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************
 * Deletes an image from a imgFS imgFS.
 */
int do_delete(const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(imgfs_file->file);

    size_t i = 0;
    while(imgfs_file->header.nb_files > 0 && i < imgfs_file->header.max_files) {

        if(imgfs_file->metadata[i].is_valid != EMPTY
           && strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {

            if(fseek(imgfs_file->file, (long) (sizeof(struct imgfs_header) +
                                               i * sizeof(struct img_metadata)),
                     SEEK_SET) != 0) {
                return ERR_IO;
            }

            imgfs_file->metadata[i].is_valid = EMPTY;

            if(fwrite(&imgfs_file->metadata[i],
                      sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
                return ERR_IO;
            }

            if(fseek(imgfs_file->file, 0, SEEK_SET) != 0) {
                return ERR_IO;
            }

            imgfs_file->header.nb_files--;
            imgfs_file->header.version++;

            if(fwrite(&imgfs_file->header,
                      sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
                return ERR_IO;
            }
            return ERR_NONE;
        }
        i++;
    }
    return ERR_IMAGE_NOT_FOUND;
}
