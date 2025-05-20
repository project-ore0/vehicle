#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"

// Ai Thinker ESP32-CAM
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#define FLASH_LED_GPIO 4

extern camera_config_t camera_config;

esp_err_t camera_init(void);

void camera_stop(void);

void camera_set_flash_duty(uint8_t duty_percent);

#endif // CAMERA_H
