#include "imgfs.h"
#include <vips/vips.h>

/*******************************************************************
 * Free the memory allocated to the buffer and the VipsImages.
 */
void free_all(void* buffer, VipsImage* image_vips_in, VipsImage* image_vips_resized)
{
    if (buffer != NULL) free(buffer);

    if (image_vips_in != NULL) g_object_unref(image_vips_in);

    if (image_vips_resized != NULL) g_object_unref(image_vips_resized);
}

/*******************************************************************
 * Resize the image to the given resolution, if needed.
 */
int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    struct imgfs_header header = imgfs_file->header;

    if(index >= header.max_files) return ERR_INVALID_IMGID;

    struct img_metadata* image = &(imgfs_file->metadata[index]);

    if(image->is_valid == EMPTY) return ERR_INVALID_IMGID;

    if (resolution == ORIG_RES) {
        return ERR_NONE;
    } else if (resolution == THUMB_RES || resolution == SMALL_RES) {
        if(image->offset[resolution] == 0) {
            VipsImage* image_vips_in = NULL;
            void* buffer = calloc(1, image->size[ORIG_RES]);

            if (buffer == NULL) return ERR_OUT_OF_MEMORY;

            if (fseek(imgfs_file->file, (long) image->offset[ORIG_RES], SEEK_SET) != 0) {
                free(buffer);
                return ERR_IO;
            }

            if (fread(buffer, 1, image->size[ORIG_RES], imgfs_file->file)
                != image->size[ORIG_RES]) {
                free(buffer);
                return ERR_IO;
            }

            if(vips_jpegload_buffer(buffer, image->size[ORIG_RES],
                                    &image_vips_in, NULL) != 0) {
                free_all(buffer, image_vips_in, NULL);
                return ERR_IMGLIB;
            }

            VipsImage* image_vips_resized = NULL;

            uint16_t resolution_index_1 = (resolution == THUMB_RES) ? 0 : 2;
            uint16_t resolution_index_2 = (resolution == THUMB_RES) ? 1 : 3;

            if(vips_thumbnail_image(image_vips_in, &image_vips_resized,
                                    header.resized_res[resolution_index_1],"height",
                                    header.resized_res[resolution_index_2], NULL) != 0) {
                free_all(buffer, image_vips_in, image_vips_resized);
                return ERR_IMGLIB;
            }

            size_t size = 0;
            if(vips_jpegsave_buffer(image_vips_resized, &buffer,
                                    &size, NULL) != 0) {
                free_all(buffer, image_vips_in, image_vips_resized);
                return ERR_IMGLIB;
            }

            image->size[resolution] = (uint32_t) size;

            if(fseek(imgfs_file->file, 0, SEEK_END) != 0) {
                free_all(buffer, image_vips_in, image_vips_resized);
                return ERR_IO;
            }

            image->offset[resolution] = (uint64_t) ftell(imgfs_file->file);

            if(fwrite(buffer, 1, image->size[resolution], imgfs_file->file)
               != image->size[resolution]) {
                free_all(buffer, image_vips_in, image_vips_resized);
                return ERR_IO;
            }

            if(fseek(imgfs_file->file, (long) (sizeof(struct imgfs_header) +
                                               index * sizeof(struct img_metadata)),
                     SEEK_SET) != 0) {
                free_all(buffer, image_vips_in, image_vips_resized);
                return ERR_IO;
            }

            if(fwrite(image, sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
                free_all(buffer, image_vips_in, image_vips_resized);
                return ERR_IO;
            }

            free_all(buffer, image_vips_in, image_vips_resized);
        }
        return ERR_NONE;
    } else {
        return ERR_INVALID_ARGUMENT;
    }
}

int get_resolution(uint32_t *height, uint32_t *width,
                   const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void*) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width  = (uint32_t) vips_image_get_width (original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}
