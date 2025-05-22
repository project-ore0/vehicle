#include "spiffs_utils.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SPIFFS_UTILS";

void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/data",
        .partition_label = "data",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
    ESP_LOGI(TAG, "SPIFFS data/ mounted");
}

void list_spiffs_files(const char *base_path) {
    ESP_LOGI(TAG, "Listing files in %s", base_path);
    DIR *dir = opendir(base_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", base_path);
        return;
    }
    struct dirent *entry;
    char path[256];
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s", entry->d_name);
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            int written = snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
            if (written < 0 || written >= sizeof(path)) {
                ESP_LOGW(TAG, "Path truncated: %s/%s", base_path, entry->d_name);
                continue;
            }
            list_spiffs_files(path);
        }
    }
    closedir(dir);
}

unsigned char *read_file_from_spiffs(const char *path, size_t *length_out) {
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        ESP_LOGE(TAG, "Out of memory reading %s", path);
        return NULL;
    }
    fread(buf, 1, len, f);
    fclose(f);
    buf[len] = '\0';
    if (length_out) *length_out = len + 1;
    return buf;
}
