#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>
#include "messages.h"

// Function to start the websocket client
void websocket_client_start(void);

// Function to stop the websocket client
void websocket_client_stop(void);

// Function to turn camera on/off
void set_camera_state(bool on);

// Function to send distance reading
void send_distance_reading(uint8_t distance);

// Function to send battery level
void send_battery_level(uint8_t percentage);

// Function to send status response
void send_status_response(void);

#endif // WEBSOCKET_CLIENT_H
