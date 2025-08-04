#include "imgfs.h"
#include "util.h"
#include <stdio.h>
#include <json-c/json.h>
#include <string.h>

/*******************************************************************
 * Displays (on stdout) imgFS metadata.
 */
int do_list(const struct imgfs_file* imgfs_file,
            enum do_list_mode output_mode, char** json)
{
    M_REQUIRE_NON_NULL(imgfs_file);

    switch (output_mode) {
    case STDOUT:
        print_header(&imgfs_file->header);
        if(imgfs_file->header.nb_files > 0) {
            if(imgfs_file->metadata != NULL) {
                size_t files_read = 0;
                size_t i = 0;
                while(i < imgfs_file->header.max_files
                      && files_read < imgfs_file->header.nb_files) {
                    if(imgfs_file->metadata[i].is_valid) {
                        print_metadata(&imgfs_file->metadata[i]);
                        ++files_read;
                    }
                    ++i;
                }
            }
        } else {
            printf("<< empty imgFS >>\n");
        }
        return ERR_NONE;
    case JSON:
        if (imgfs_file->header.nb_files > 0) {
            json_object* jobj = json_object_new_object();
            json_object* jarray = json_object_new_array();
            size_t files_read = 0;
            size_t i = 0;
            while (i < imgfs_file->header.max_files
                   && files_read < imgfs_file->header.nb_files) {
                if (imgfs_file->metadata[i].is_valid) {
                    json_object_array_add(jarray, json_object_new_string(imgfs_file->metadata[i].img_id));
                    ++files_read;
                }
                ++i;
            }
            json_object_object_add(jobj, "Images", jarray);
            *json = strdup(json_object_to_json_string(jobj));
            json_object_put(jobj);
        } else {
            json_object* jobj = json_object_new_object();
            json_object_object_add(jobj, "Images", json_object_new_array());
            *json = strdup(json_object_to_json_string(jobj));
            json_object_put(jobj);
        }
        return ERR_NONE;
    default:
        return ERR_DEBUG;
    }
}
