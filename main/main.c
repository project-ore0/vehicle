// PlatformIO project: esp32-cam-https
// Framework: ESP-IDF
// Pure C version with WiFi connection monitoring and fallback

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_spiffs.h"
#include "string.h"
#include "esp_system.h"
#include "esp_netif.h"
#include <dirent.h>

#include "config.h"
#include "wifi_manager.h"
#include "captive_portal.h"
#include "app_server.h"
#include "spiffs_utils.h"
#include "websocket_client.h"

static const char *TAG = "HTTPS_APP";


//----------------------------------------------------------------------
// App main
//----------------------------------------------------------------------

esp_err_t _captive_portal_callback(const char *ssid, const char *password, const char *ws_uri)
{
    ESP_LOGI(TAG, "Captive portal callback: SSID=%s, Password=%s, WebSocket URI=%s", ssid, password, ws_uri);
    if (save_wifi_config(ssid, password) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save WiFi credentials");
        return ESP_FAIL;
    }
    if (save_ws_uri(ws_uri) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save WebSocket URI");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void _start_captive_portal(void)
{
    ESP_LOGI(TAG, "Starting captive portal...");
    start_captive_portal(PROJECT_NAME, PROJECT_PASSWORD, _captive_portal_callback);
}

void app_main(void)
{
    esp_log_level_set("wifi", ESP_LOG_NONE);
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    init_spiffs();
    list_spiffs_files("/spiffs");

    char ssid[64];
    char pass[64];
    if (load_wifi_config(ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK)
    {
        ESP_LOGI(TAG, "No WiFi config, starting captive portal");
        _start_captive_portal();
    }
    else
    {
        ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
        if (wifi_connect(ssid, pass))
        {
            ESP_LOGI(TAG, "WiFi connected, launching WebSocket client and HTTPS server");
            websocket_client_start();
            start_https_server();
        }
        else
        {
            ESP_LOGW(TAG, "WiFi connection failed, launching captive portal");
            esp_wifi_stop();
            vTaskDelay(pdMS_TO_TICKS(1000));
            _start_captive_portal();
        }
    }
}
