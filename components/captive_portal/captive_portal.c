
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_https_server.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdlib.h>

#include "httpx_server.h"
#include "spiffs_utils.h"
#include "captive_portal.h"

#ifndef ESP_RETURN_ON_ERROR
#define ESP_RETURN_ON_ERROR(_rc) do { \
    esp_err_t rc = (esp_err_t)(_rc); \
    if (rc != ESP_OK) { \
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(rc)); \
        return rc; \
    } \
} while (0)
#endif

static const char *TAG = "CAPTIVE_PORTAL";

static httpx_server_t *captive_http_server = NULL;

bool _rebooting = false;
esp_err_t (*_callback)(const char* ssid, const char* password, const char* ws_uri) = NULL;

// macro for generating HTML document
#define HTML_DOC(title, body) \
    "<!DOCTYPE html>" \
    "<html>" \
    "<head>" \
    "<title>" title "</title>" \
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">" \
    "</head>" \
    "<body>" \
    "<h2>" title "<h2>" \
    body \
    "</body>" \
    "</html>"

static esp_err_t handle_root(httpd_req_t *req) {
    const char *html = HTML_DOC(
        "Provision WiFi",
        "<form action=\"/save\" method=\"post\">"
        "SSID: <input name='ssid'/><br>"
        "Password: <input name='password' type='password'/><br>"
        "WebSocket: <input name='ws' type='text' value=\"wss://ore0.lazos.me/wsc\"/><br>"
        "<input type='submit' value='Save'/>"
        "</form>"
    );
    if (_rebooting) {
        html = HTML_DOC(
            "Provision WiFi",
            "<h4>Saved, rebooting...</h4>"
        );
    }
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handle_save(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = 0;

    char ssid[64] = {0};
    char pass[64] = {0};
    char ws[128] = {0};
    
    httpx_parse_form_field(buf, "ssid", ssid, sizeof(ssid));
    httpx_parse_form_field(buf, "password", pass, sizeof(pass));
    httpx_parse_form_field(buf, "ws", ws, sizeof(ws));

    if(_callback(ssid, pass, ws) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi credentials");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save settings");
        return ESP_OK;
    }

    // respond with location redirect to the root page
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_send(req, NULL, 0);
    
    // reboot device
    _rebooting = true;
    ESP_LOGI(TAG, "Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

esp_err_t start_softap(const char *ssid, const char *password) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg));
    wifi_config_t ap_config = { 
        .ap = {
            .ssid = {0},
            .password = {0},
            .ssid_len = 0,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0
        }
     };
    strncpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid[sizeof(ap_config.ap.ssid) - 1] = '\0';
    ap_config.ap.ssid_len = strlen((const char *)ap_config.ap.ssid);
    strncpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password));
    ap_config.ap.password[sizeof(ap_config.ap.password) - 1] = '\0';
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_RETURN_ON_ERROR(esp_wifi_start());
    return ESP_OK;
}

void start_captive_portal(char *ssid, const char *password, esp_err_t (*callback)(const char* ssid, const char* password, const char* ws_uri)) {
    _callback = callback;
    _rebooting = false;
    ESP_LOGI(TAG, "Starting captive portal...");

    ESP_ERROR_CHECK(start_softap(ssid, password));
    ESP_LOGI(TAG, "SoftAP started");
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) {
        ESP_LOGE(TAG, "Failed to get AP netif");
        return;
    }
    // log the AP IP address
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "AP IP: " IPSTR, IP2STR(&ip_info.ip));
    } else {
        ESP_LOGE(TAG, "Failed to get AP IP");
        return;
    }

    if(captive_http_server) {
        ESP_LOGI(TAG, "Stopping existing captive portal server");
        httpx_server_destroy(captive_http_server);
        captive_http_server = NULL;
    }
    ESP_ERROR_CHECK(httpx_server_create(&captive_http_server));

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = handle_root};
    httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = handle_save};
    ESP_ERROR_CHECK(httpd_register_uri_handler(captive_http_server->https, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(captive_http_server->https, &save));

    ESP_LOGI(TAG, "Captive portal ready (HTTPS)");
}

void stop_captive_portal(void) {
    if (captive_http_server) {
        httpx_server_destroy(captive_http_server);
        captive_http_server = NULL;
    }
    esp_wifi_stop();
}
