#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "http_prot.h"
#include "imgfs.h"
#include "util.h"

int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(target_uri);

    for (size_t i = 0; i < strlen(target_uri); i++) {
        if (message->uri.val[i] != target_uri[i]) {
            return 0;
        }
    }
    return 1;
}

int http_match_verb(const struct http_string* method, const char* verb)
{
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);

    if (method->len != strlen(verb)) {
        return 0;
    }
    for (size_t i = 0; i < strlen(verb); i++) {
        if (method->val[i] != verb[i]) {
            return 0;
        }
    }
    return 1;
}

int http_get_var(const struct http_string* url, const char* name,
                 char* out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);

    char new_name[strlen(name) + 2];
    snprintf(new_name, sizeof(new_name), "%s=", name);

    size_t i = 0;
    while (i < url->len) {
        if (url->val[i] == new_name[0]) {
            size_t j = 0;
            while (j < strlen(new_name)) {
                if (url->val[i+j] != new_name[j]) {
                    break;
                }
                j++;
            }
            if (j == strlen(new_name)) {
                size_t k = 0;
                while (url->val[i+j+k] != '&' && i+j+k < url->len && k < out_len) {
                    out[k] = url->val[i+j+k];
                    k++;
                }
                if (k >= out_len) {
                    return ERR_RUNTIME;
                }
                out[k] = '\0';
                return k;
            }
        }
        i++;
    }
    return 0;
}

static const char* get_next_token(const char* message, const char* delimiter,
                                  struct http_string* output)
{
    if(message == NULL || delimiter == NULL) return NULL;

    const char* token_start = strstr(message, delimiter);

    if (token_start == NULL) return NULL;

    if (output != NULL) {
        output->val = message;
        output->len = token_start - message;
    }

    return token_start + strlen(delimiter);
}

static const char* http_parse_headers(const char* header_start,
                                      struct http_message* output)
{
    if (header_start == NULL || output == NULL) return NULL;

    const char* current = header_start;
    while (strncmp(current, HTTP_LINE_DELIM, strlen(HTTP_LINE_DELIM)) != 0) {
        struct http_header* header = &output->headers[output->num_headers++];
        if (header != NULL) {
            current = get_next_token(current, HTTP_HDR_KV_DELIM, &header->key);
            current = get_next_token(current, HTTP_LINE_DELIM, &header->value);
        }
    }
    return current + strlen(HTTP_LINE_DELIM);
}

int http_parse_message(const char *stream, size_t bytes_received,
                       struct http_message *out, int *content_len)
{
    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);

    const char* header_end = strstr(stream, HTTP_HDR_END_DELIM);

    if (header_end == NULL) return 0;

    struct http_string method;
    struct http_string uri;
    stream = get_next_token(stream, " ", &method);
    stream = get_next_token(stream, " ", &uri);

    if (stream == NULL) return 0;

    stream = get_next_token(stream, HTTP_LINE_DELIM, NULL);

    if (stream == NULL) return 0;

    out->method = method;
    out->uri = uri;
    out->num_headers = 0;

    stream = http_parse_headers(stream, out);

    if (stream == NULL) return 0;

    size_t content_length = 0;
    for (size_t i = 0; i < out->num_headers; i++) {
        if (strncmp(out->headers[i].key.val, "Content-Length", out->headers[i].key.len) == 0) {
            content_length = atoi(out->headers[i].value.val);
            break;
        }
    }

    *content_len = content_length;

    if (content_length == 0) {
        out->body.val = NULL;
        out->body.len = 0;
        return 1;
    } else {
        out->body.val = stream;
        out->body.len = strlen(stream);

        return (content_length == out->body.len) ? 1 : 0;
    }
}
