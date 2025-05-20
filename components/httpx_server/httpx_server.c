#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "spiffs_utils.h"
#include "httpx_server.h"

static const char *TAG = "HTTPX_SERVER";

static int _hex2int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return 0;
}

static esp_err_t _get_ip_info(char *ifkey, esp_netif_ip_info_t *ip_info) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(ifkey);
    if(netif) {
        esp_err_t ret = esp_netif_get_ip_info(netif, ip_info);
        if (ret == ESP_OK && ip_info->ip.addr != 0) {
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static void _get_redirect_url(char *url, size_t len) {
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    esp_netif_ip_info_t ip_info;
    // get wifi mode
    wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode) == ESP_OK) {
        switch(mode) {
            case WIFI_MODE_AP:
                ret = _get_ip_info("WIFI_AP", &ip_info);
                break;
            case WIFI_MODE_STA:
                ret = _get_ip_info("WIFI_STA", &ip_info);
                break;
            default:
                snprintf(url, len, "https://192.168.4.2/");
                break;
        }
    }
    if (ret == ESP_OK) {
        snprintf(url, len, "https://%d.%d.%d.%d/", IP2STR(&ip_info.ip));
    }
}

static esp_err_t _handle_redirect_https(httpd_req_t *req) {
    static char redirect_url[64];
    _get_redirect_url(redirect_url, sizeof(redirect_url));
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    httpd_resp_send(req, "Redirecting to captive portal...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t httpx_server_create(httpx_server_t **server) {
    *server = calloc(1, sizeof(httpx_server_t));
    if (!*server) {
        return ESP_ERR_NO_MEM;
    }
    (*server)->http = NULL;
    (*server)->https = NULL;

    // load certificate and key from SPIFFS
    size_t cert_len, key_len;
    unsigned char *cert = read_file_from_spiffs("/spiffs/certs/crt.pem", &cert_len);
    unsigned char *key = read_file_from_spiffs("/spiffs/certs/key.pem", &key_len);
    if (!cert || !key) {
        ESP_LOGE(TAG, "Could not read certificate or key from SPIFFS");
        free(*server);
        *server = NULL;
        return ESP_ERR_INVALID_ARG;
    }
    // print cert to serial out
    ESP_LOGI(TAG, "Certificate: %.*s", cert_len, cert);

    // start http server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t ret = httpd_start(&(*server)->http, &config);
    if (ret != ESP_OK) {
        free(*server);
        *server = NULL;
        return ret;
    }
    ret = httpd_register_uri_handler((*server)->http, &(httpd_uri_t) {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = _handle_redirect_https,
        .user_ctx = NULL
    });
    if (ret != ESP_OK) {
        httpd_stop((*server)->http);
        free(*server);
        *server = NULL;
        return ret;
    }

    // start https server
    httpd_ssl_config_t https_config = HTTPD_SSL_CONFIG_DEFAULT();
    https_config.servercert = cert;
    https_config.servercert_len = cert_len;
    https_config.prvtkey_pem = key;
    https_config.prvtkey_len = key_len;
    https_config.httpd.uri_match_fn = httpd_uri_match_wildcard;
    ret = httpd_ssl_start(&(*server)->https, &https_config);
    free(cert);
    free(key);
    if (ret != ESP_OK) {
        httpd_stop((*server)->http);
        free(*server);
        *server = NULL;
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t httpx_server_destroy(httpx_server_t *server) {
    if (server->http) {
        httpd_stop(server->http);
        server->http = NULL;
    }
    if (server->https) {
        httpd_ssl_stop(server->https);
        server->https = NULL;
    }
    free(server);
    return ESP_OK;
}

esp_err_t httpx_uri_decode(char *dst, const char *src, size_t dst_size) {
    char a, b;
    size_t di = 0;
    while (*src && di + 1 < dst_size) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            dst[di++] = (char)((_hex2int(a) << 4) | _hex2int(b));
            src += 3;
        } else if (*src == '+') {
            dst[di++] = ' ';
            src++;
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
    return (di < dst_size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}


esp_err_t httpx_parse_form_field(const char *body, const char *key, char *out, size_t out_len) {
    if (!body || !key || !out || out_len == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    size_t key_len = strlen(key);
    const char *p = strstr(body, key);
    if (!p) {
        ESP_LOGE(TAG, "Key not found in body");
        return ESP_ERR_HTTPD_INVALID_REQ;
    }
    p += key_len;
    if (*p != '='){
        ESP_LOGE(TAG, "Key not followed by '='");
        return ESP_ERR_HTTPD_INVALID_REQ;
    }
    char out_tmp[512];
    p++;
    size_t j = 0;
    for (; *p && *p != '&' && j < sizeof(out_tmp) - 1; ++p) {
        if (*p == '+') out_tmp[j++] = ' ';
        else if (*p == '%' && p[1] && p[2]) {
            char hex[3] = {p[1], p[2], 0};
            out_tmp[j++] = (char)strtol(hex, NULL, 16);
            p += 2;
        } else out_tmp[j++] = *p;
    }
    out_tmp[j] = 0;
    if (j == 0) {
        ESP_LOGE(TAG, "No data parsed");
        return ESP_ERR_NOT_FOUND;
    }
    // decode the value
    if (httpx_uri_decode(out, out_tmp, out_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode value");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}
