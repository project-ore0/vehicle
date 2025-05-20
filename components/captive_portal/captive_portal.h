#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

void start_captive_portal(char *ssid, const char *password, esp_err_t (*callback)(const char* ssid, const char* password, const char* ws_uri));
void stop_captive_portal(void);

#endif // CAPTIVE_PORTAL_H
