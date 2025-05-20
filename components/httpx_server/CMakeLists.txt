idf_component_register(
    SRCS "httpx_server.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server esp_https_server esp_netif esp_wifi spiffs_utils
)