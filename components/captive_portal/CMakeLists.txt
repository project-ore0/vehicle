idf_component_register(
    SRCS "captive_portal.c"
    INCLUDE_DIRS "."
    REQUIRES esp_netif esp_wifi spiffs spiffs_utils httpx_server
)