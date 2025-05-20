#ifndef CONFIG_H
#define CONFIG_H

#include "esp_err.h"

#define PROJECT_NAME "ORE0"
#define PROJECT_PASSWORD "ore0pass"

#define CONFIG_NAMESPACE "config"

#define CONFIG_KEY_WS_URI "ws_uri"
#define CONFIG_KEY_WIFI_SSID "ssid"
#define CONFIG_KEY_WIFI_PASS "password"

#define CONFIG_KEY_MOTOR1A "motor1a"
#define CONFIG_KEY_MOTOR1B "motor1b"
#define CONFIG_KEY_MOTOR2A "motor2a"
#define CONFIG_KEY_MOTOR2B "motor2b"

#define CONFIG_DEFAULT_MOTOR1A 12
#define CONFIG_DEFAULT_MOTOR1B 13
#define CONFIG_DEFAULT_MOTOR2A 14
#define CONFIG_DEFAULT_MOTOR2B 15


esp_err_t save_ws_uri(const char *ws_uri);
esp_err_t load_ws_uri(char *ws_uri, size_t ws_uri_len);
esp_err_t clear_ws_uri(void);

esp_err_t save_wifi_config(const char *ssid, const char *password);
esp_err_t load_wifi_config(char *ssid, size_t ssid_len, char *password, size_t pass_len);
esp_err_t clear_wifi_config(void);

esp_err_t save_motor_config(uint8_t motor1a, uint8_t motor1b, uint8_t motor2a, uint8_t motor2b);
esp_err_t load_motor_config(uint8_t *motor1a, uint8_t *motor1b, uint8_t *motor2a, uint8_t *motor2b);
esp_err_t clear_motor_config(void);

#endif // CONFIG_H