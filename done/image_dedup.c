#include "imgfs.h"
#include <stdint.h>
#include <string.h>

/*******************************************************************
 * Image deduplication.
 */
int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if(index >= imgfs_file->header.max_files) return ERR_IMAGE_NOT_FOUND;

    struct img_metadata* image = &(imgfs_file->metadata[index]);

    if(image->is_valid == EMPTY) return ERR_IMAGE_NOT_FOUND;

    int has_duplicated_content = 0;

    for(uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
        if(i != index) {
            struct img_metadata* other_image = &(imgfs_file->metadata[i]);
            if(other_image->is_valid) {
                if(strcmp(image->img_id, other_image->img_id) == 0) {
                    return ERR_DUPLICATE_ID;
                }
                if(memcmp(image->SHA, other_image->SHA, SHA256_DIGEST_LENGTH) == 0) {
                    image->size[ORIG_RES] = other_image->size[ORIG_RES];
                    image->size[THUMB_RES] = other_image->size[THUMB_RES];
                    image->size[SMALL_RES] = other_image->size[SMALL_RES];
                    image->offset[ORIG_RES] = other_image->offset[ORIG_RES];
                    image->offset[THUMB_RES] = other_image->offset[THUMB_RES];
                    image->offset[SMALL_RES] = other_image->offset[SMALL_RES];
                    has_duplicated_content = 1;
                }
            }

        }
    }

    if (has_duplicated_content == 0) image->offset[ORIG_RES] = 0;

    return ERR_NONE;
}
