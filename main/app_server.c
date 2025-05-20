#include "app_server.h"
#include "spiffs_utils.h"
#include "esp_https_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "httpx_server.h"
#include "config.h"

static const char *TAG = "HTTPS_SERVER";

#define APP_SERVER_FS_ROOT "/spiffs/web"
#define APP_SERVER_INDEX "index.html"

static httpx_server_t *app_http_server = NULL;


static void render_index_html_with_values(httpd_req_t *req, const char *html_path) {
    char ws_uri[128] = {0};
    uint8_t motor1a = CONFIG_DEFAULT_MOTOR1A;
    uint8_t motor1b = CONFIG_DEFAULT_MOTOR1B;
    uint8_t motor2a = CONFIG_DEFAULT_MOTOR2A;
    uint8_t motor2b = CONFIG_DEFAULT_MOTOR2B;
    load_ws_uri(ws_uri, sizeof(ws_uri));
    load_motor_config(&motor1a, &motor1b, &motor2a, &motor2b);

    size_t html_len = 0;
    char *html_template = (char *)read_file_from_spiffs(html_path, &html_len);
    if (!html_template) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load HTML template");
        return;
    }

    // Prepare string values for replacement
    char motor1a_str[8], motor1b_str[8], motor2a_str[8], motor2b_str[8];
    snprintf(motor1a_str, sizeof(motor1a_str), "%d", motor1a);
    snprintf(motor1b_str, sizeof(motor1b_str), "%d", motor1b);
    snprintf(motor2a_str, sizeof(motor2a_str), "%d", motor2a);
    snprintf(motor2b_str, sizeof(motor2b_str), "%d", motor2b);

    // Allocate output buffer (same size as template + extra for values)
    size_t out_size = html_len + 128;
    char *out = malloc(out_size);
    if (!out) {
        free(html_template);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return;
    }
    // Simple templating: replace {{ ws_uri }}, {{ motor1a }}, etc.
    const struct { const char *key; const char *val; } replacements[] = {
        { "{{ ws_uri }}", ws_uri },
        { "{{ motor1a }}", motor1a_str },
        { "{{ motor1b }}", motor1b_str },
        { "{{ motor2a }}", motor2a_str },
        { "{{ motor2b }}", motor2b_str },
    };
    char *src = html_template;
    char *dst = out;
    size_t remain = out_size - 1;
    while (*src && remain > 0) {
        int replaced = 0;
        for (size_t i = 0; i < sizeof(replacements)/sizeof(replacements[0]); ++i) {
            size_t klen = strlen(replacements[i].key);
            if (strncmp(src, replacements[i].key, klen) == 0) {
                size_t vlen = strlen(replacements[i].val);
                if (vlen > remain) vlen = remain;
                memcpy(dst, replacements[i].val, vlen);
                dst += vlen;
                src += klen;
                remain -= vlen;
                replaced = 1;
                break;
            }
        }
        if (!replaced) {
            *dst++ = *src++;
            --remain;
        }
    }
    *dst = '\0';
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(html_template);
    free(out);
}

static const char *get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    ext++;
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0)
        return "text/html";
    if (strcasecmp(ext, "txt") == 0)
        return "text/plain";
    if (strcasecmp(ext, "css") == 0)
        return "text/css";
    if (strcasecmp(ext, "js") == 0)
        return "application/javascript";
    if (strcasecmp(ext, "ico") == 0)
        return "image/x-icon";
    if (strcasecmp(ext, "png") == 0)
        return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0)
        return "image/gif";
    return "application/octet-stream";
}

static esp_err_t fs_handle_file(httpd_req_t *req) {
    if (strcmp(req->uri, "/") == 0 || strcmp(req->uri, "/" APP_SERVER_INDEX) == 0) {
        render_index_html_with_values(req,  APP_SERVER_FS_ROOT "/" APP_SERVER_INDEX);
        return ESP_OK;
    }

    char path[128];
    size_t base_len = strlen(APP_SERVER_FS_ROOT);
    size_t max_uri_len = sizeof(path) - base_len - 1;
    snprintf(path, sizeof(path), APP_SERVER_FS_ROOT);
    strncat(path, req->uri, max_uri_len);
    if (path[strlen(path) - 1] == '/')
        strncat(path, APP_SERVER_INDEX, sizeof(path) - strlen(path) - 1);
    FILE *file = fopen(path, "r");
    if (!file) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buf = (char *)malloc(len);
    fread(buf, 1, len, file);
    fclose(file);
    httpd_resp_set_type(req, get_content_type(path));
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }

    char section[32] = {0};
    httpx_parse_form_field(buf, "section", section, sizeof(section));
    ESP_LOGI(TAG, "Saving config data: %s", buf);

    if (strcmp(section, "ws") == 0) {
        char ws_uri[128] = {0};
        esp_err_t err = httpx_parse_form_field(buf, "ws_uri", ws_uri, sizeof(ws_uri));
        if (err == ESP_OK) {
            if(save_ws_uri(ws_uri) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save WebSocket URI: %s", ws_uri);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save WebSocket URI");
                return ESP_OK;
            }
        } else {
            ESP_LOGE(TAG, "Failed to parse ws_uri field: err=%d", err);
        }
    } else if (strcmp(section, "motor") == 0) {
        char motor1a[8] = {0};
        char motor1b[8] = {0};
        char motor2a[8] = {0};
        char motor2b[8] = {0};
        esp_err_t err;
        err = httpx_parse_form_field(buf, "motor1a", motor1a, sizeof(motor1a));
        if (err == ESP_OK) err = httpx_parse_form_field(buf, "motor1b", motor1b, sizeof(motor1b));
        if (err == ESP_OK) err = httpx_parse_form_field(buf, "motor2a", motor2a, sizeof(motor2a));
        if (err == ESP_OK) err = httpx_parse_form_field(buf, "motor2b", motor2b, sizeof(motor2b));
        if (err == ESP_OK) {
            uint8_t motor1a_val = (uint8_t)atoi(motor1a);
            uint8_t motor1b_val = (uint8_t)atoi(motor1b);
            uint8_t motor2a_val = (uint8_t)atoi(motor2a);
            uint8_t motor2b_val = (uint8_t)atoi(motor2b);
            if(save_motor_config(motor1a_val, motor1b_val, motor2a_val, motor2b_val) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save motor config: %s", buf);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save motor config");
                return ESP_OK;
            }
        } else {
            ESP_LOGE(TAG, "Failed to parse one or more motor fields: err=%d", err);
        }
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t clear_handler(httpd_req_t *req) {
    clear_ws_uri();
    clear_wifi_config();
    clear_motor_config();
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void start_https_server(void) {
    if (app_http_server) {
        ESP_LOGI(TAG, "Stopping existing HTTPS server");
        httpx_server_destroy(app_http_server);
        app_http_server = NULL;
    }
    ESP_ERROR_CHECK(httpx_server_create(&app_http_server));

    httpd_uri_t fs_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = fs_handle_file,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(app_http_server->https, &fs_uri));

    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler,
    };
    httpd_uri_t clear_uri = {
        .uri = "/clear",
        .method = HTTP_POST,
        .handler = clear_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(app_http_server->https, &save_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(app_http_server->https, &clear_uri));

    ESP_LOGI(TAG, "HTTPS server started");
}
