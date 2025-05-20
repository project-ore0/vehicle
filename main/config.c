#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"

#include "config.h"

static const char *TAG = "CONFIG";

// Helper to open NVS
static esp_err_t _open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode) {
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, mode, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
    return err;
}

// Helper to set a string key
static esp_err_t _set_str_key(nvs_handle_t handle, const char *key, const char *value, const char *log_label) {
    esp_err_t err = nvs_set_str(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", log_label, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

// Helper to set a u8 key
static esp_err_t _set_u8_key(nvs_handle_t handle, const char *key, uint8_t value, const char *log_label) {
    esp_err_t err = nvs_set_u8(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", log_label, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

// Helper to get a string key with buffer size check
static esp_err_t _get_str_key(nvs_handle_t handle, const char *key, char *buf, size_t buf_len, const char *log_label) {
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(handle, key, NULL, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get %s length: %s", log_label, esp_err_to_name(err));
        return err;
    }
    if (required_size > buf_len) {
        ESP_LOGE(TAG, "%s buffer too small. Needed: %d, Provided: %d", log_label, required_size, buf_len);
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    err = nvs_get_str(handle, key, buf, &required_size);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

// Helper to get a u8 key
static esp_err_t _get_u8_key(nvs_handle_t handle, const char *key, uint8_t *value, const char *log_label) {
    esp_err_t err = nvs_get_u8(handle, key, value);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

// Helper to erase a key
static esp_err_t _erase_key(nvs_handle_t handle, const char *key, const char *log_label) {
    esp_err_t err = nvs_erase_key(handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear %s: %s", log_label, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t save_ws_uri(const char *ws_uri) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READWRITE);
    if (err != ESP_OK) return err;
    err = _set_str_key(handle, CONFIG_KEY_WS_URI, ws_uri, "WebSocket URI");
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "WebSocket URI saved: %s", ws_uri);
    }
    nvs_close(handle);
    return err;
}

esp_err_t load_ws_uri(char *ws_uri, size_t ws_uri_len) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READONLY);
    if (err != ESP_OK) return err;
    err = _get_str_key(handle, CONFIG_KEY_WS_URI, ws_uri, ws_uri_len, "WebSocket URI");
    nvs_close(handle);
    return err;
}

esp_err_t clear_ws_uri(void) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READWRITE);
    if (err != ESP_OK) return err;
    err = _erase_key(handle, CONFIG_KEY_WS_URI, "WebSocket URI");
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "WebSocket URI cleared");
    }
    nvs_close(handle);
    return err;
}

esp_err_t save_wifi_config(const char *ssid, const char *password) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READWRITE);
    if (err != ESP_OK) return err;
    err = _set_str_key(handle, CONFIG_KEY_WIFI_SSID, ssid, "SSID");
    if (err == ESP_OK) err = _set_str_key(handle, CONFIG_KEY_WIFI_PASS, password, "password");
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "WiFi credentials saved: SSID=%s, Password=%s", ssid, password);
    }
    nvs_close(handle);
    return err;
}

esp_err_t load_wifi_config(char *ssid, size_t ssid_len, char *password, size_t pass_len) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READONLY);
    if (err != ESP_OK) return err;
    err = _get_str_key(handle, CONFIG_KEY_WIFI_SSID, ssid, ssid_len, "SSID");
    if (err == ESP_OK) err = _get_str_key(handle, CONFIG_KEY_WIFI_PASS, password, pass_len, "password");
    nvs_close(handle);
    return err;
}

esp_err_t clear_wifi_config(void) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READWRITE);
    if (err != ESP_OK) return err;
    err = _erase_key(handle, CONFIG_KEY_WIFI_SSID, "SSID");
    if (err == ESP_OK) err = _erase_key(handle, CONFIG_KEY_WIFI_PASS, "password");
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "WiFi credentials cleared");
    }
    nvs_close(handle);
    return err;
}

esp_err_t save_motor_config(uint8_t motor1a, uint8_t motor1b, uint8_t motor2a, uint8_t motor2b) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READWRITE);
    if (err != ESP_OK) return err;
    err = _set_u8_key(handle, CONFIG_KEY_MOTOR1A, motor1a, "Motor 1A");
    if (err == ESP_OK) err = _set_u8_key(handle, CONFIG_KEY_MOTOR1B, motor1b, "Motor 1B");
    if (err == ESP_OK) err = _set_u8_key(handle, CONFIG_KEY_MOTOR2A, motor2a, "Motor 2A");
    if (err == ESP_OK) err = _set_u8_key(handle, CONFIG_KEY_MOTOR2B, motor2b, "Motor 2B");
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Motor configuration saved: M1A=%d, M1B=%d, M2A=%d, M2B=%d", motor1a, motor1b, motor2a, motor2b);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t _get_motor_key(nvs_handle_t handle, const char *key, uint8_t *value, uint8_t default_value) {
    if(_get_u8_key(handle, key, value, "Motor") != ESP_OK) {
        *value = default_value;
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

esp_err_t load_motor_config(uint8_t *motor1a, uint8_t *motor1b, uint8_t *motor2a, uint8_t *motor2b) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READONLY);
    if (err != ESP_OK) return err;
    _get_motor_key(handle, CONFIG_KEY_MOTOR1A, motor1a, CONFIG_DEFAULT_MOTOR1A);
    _get_motor_key(handle, CONFIG_KEY_MOTOR1B, motor1b, CONFIG_DEFAULT_MOTOR1B);
    _get_motor_key(handle, CONFIG_KEY_MOTOR2A, motor2a, CONFIG_DEFAULT_MOTOR2A);
    _get_motor_key(handle, CONFIG_KEY_MOTOR2B, motor2b, CONFIG_DEFAULT_MOTOR2B);
    nvs_close(handle);
    return err;
}

esp_err_t clear_motor_config(void) {
    nvs_handle_t handle;
    esp_err_t err = _open_nvs(&handle, NVS_READWRITE);
    if (err != ESP_OK) return err;
    err = _erase_key(handle, "motor1a", "Motor 1A");
    if (err == ESP_OK) err = _erase_key(handle, "motor1b", "Motor 1B");
    if (err == ESP_OK) err = _erase_key(handle, "motor2a", "Motor 2A");
    if (err == ESP_OK) err = _erase_key(handle, "motor2b", "Motor 2B");
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Motor configuration cleared");
    }
    nvs_close(handle);
    return err;
}
