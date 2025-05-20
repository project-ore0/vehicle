#ifndef MISC_H
#define MISC_H

#include "esp_err.h"
#include "esp_log.h"


#define ESP_RETURN_ON_ERROR(_rc) do { \
    esp_err_t rc = (esp_err_t)(_rc); \
    if (rc != ESP_OK) { \
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(rc)); \
        return rc; \
    } \
} while (0)


#endif // MISC_H