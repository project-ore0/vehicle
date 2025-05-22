idf_component_register(
    SRCS "httpx_server.c"
    INCLUDE_DIRS "."
    REQUIRES spiffs esp_http_server esp_https_server esp_netif esp_wifi
)