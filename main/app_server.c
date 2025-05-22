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
#include "app_files_handler.h"
#include "app_files.h"

static const char *TAG = "APP_SERVER";

static httpx_server_t *app_http_server = NULL;


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
            err = app_files_template_context_set("ws_uri", ws_uri);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set WebSocket URI in template context: %s", ws_uri);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set WebSocket URI in template context");
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
            app_files_template_context_set("motor1a", motor1a);
            app_files_template_context_set("motor1b", motor1b);
            app_files_template_context_set("motor2a", motor2a);
            app_files_template_context_set("motor2b", motor2b);
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

    char ws_uri[128];
    load_ws_uri(ws_uri, sizeof(ws_uri));
    app_files_template_context_set("ws_uri", ws_uri);
    uint8_t m1a, m1b, m2a, m2b;
    load_motor_config(&m1a, &m1b, &m2a, &m2b);
    // convert to string
    char cm1a[8], cm1b[8], cm2a[8], cm2b[8];
    snprintf(cm1a, sizeof(cm1a), "%d", m1a);
    snprintf(cm1b, sizeof(cm1b), "%d", m1b);
    snprintf(cm2a, sizeof(cm2a), "%d", m2a);
    snprintf(cm2b, sizeof(cm2b), "%d", m2b);
    app_files_template_context_set("motor1a", cm1a);
    app_files_template_context_set("motor1b", cm1b);
    app_files_template_context_set("motor2a", cm2a);
    app_files_template_context_set("motor2b", cm2b);
    
    ESP_ERROR_CHECK(httpx_server_create(&app_http_server));

    httpd_uri_t fs_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = app_files_handler,
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
