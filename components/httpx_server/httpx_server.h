#ifndef HTTPX_SERVER_H
#define HTTPX_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include <ctype.h>

typedef struct {
    httpd_handle_t http;
    httpd_handle_t https;
} httpx_server_t;

esp_err_t httpx_server_create(httpx_server_t **server);
esp_err_t httpx_server_destroy(httpx_server_t *server);

esp_err_t httpx_uri_decode(char *dst, const char *src, size_t dst_size);
esp_err_t httpx_parse_form_field(const char *body, const char *key, char *out, size_t out_len);

#endif // HTTPX_SERVER_H