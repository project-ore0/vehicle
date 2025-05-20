#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#define WIFI_NAMESPACE "wifi_creds"
#define KEY_SSID "ssid"
#define KEY_PASS "password"

esp_err_t save_wifi_config(const char *ssid, const char *password);
esp_err_t load_wifi_config(char *ssid, size_t ssid_len, char *password, size_t pass_len);
bool wifi_connect(const char *ssid, const char *password);
void wifi_clear_credentials(void);

#endif // WIFI_MANAGER_H
