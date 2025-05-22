#include "esp_err.h"
#include "esp_log.h"

#include "app_files.h"
#include "app_files_handler.h"
#include "esp_http_server.h"

static const char *TAG = "app_files";

static app_files_template_context_t *app_files_template_context = NULL;

esp_err_t app_files_handler(httpd_req_t *req) {
    // create a local copy of req->uri
    char uri[CONFIG_HTTPD_MAX_URI_LEN];
    strncpy(uri, req->uri, CONFIG_HTTPD_MAX_URI_LEN);
    uri[CONFIG_HTTPD_MAX_URI_LEN - 1] = '\0'; // Ensure null-termination
    ESP_LOGI(TAG, "Request URI: %s", uri);

    // append index.html if the request URI ends with "/"
    if (uri[strlen(uri) - 1] == '/') {
        strncat(uri, "index.html", CONFIG_HTTPD_MAX_URI_LEN - strlen(uri) - 1);
        ESP_LOGI(TAG, "Modified URI: %s", uri);
    }

    // Check if the request URI matches any of the app files
    for (size_t i = 0; i < app_files_count; i++) {
        if (strcmp(uri, app_files[i].uri) == 0) {
            httpd_resp_set_type(req, app_files[i].content_type);
            size_t file_size = app_files[i].end - app_files[i].start;
            
            // Check if this is an HTML file and we have a template context
            if ((strcmp(app_files[i].content_type, "text/html") == 0 ||
                 strcmp(app_files[i].content_type, "text/htm") == 0)) {
                ESP_LOGI(TAG, "Rendering HTML template: %s", uri);
                
                // Render the template
                size_t rendered_size = 0;
                char *rendered_content = app_files_render_template(
                    (const char *)app_files[i].start, 
                    file_size, 
                    app_files_template_context, 
                    &rendered_size
                );
                
                if (rendered_content) {
                    // Send the rendered content
                    httpd_resp_send(req, rendered_content, rendered_size);
                    free(rendered_content);
                    return ESP_OK;
                } else {
                    ESP_LOGE(TAG, "Failed to render template: %s", uri);
                    // Fall back to sending the original content
                }
            }
            
            // Send the original content
            httpd_resp_send(req, (const char *)app_files[i].start, file_size);
            return ESP_OK;
        }
    }

    // If no matching file is found, send a 404 Not Found response
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    ESP_LOGE(TAG, "File not found: %s", uri);
    
    return ESP_OK;
}

esp_err_t app_files_template_context_set(const char *key, const char *value) {
    app_files_template_context_t *entry;
    HASH_FIND_STR(app_files_template_context, key, entry);
    if (entry == NULL) {
        entry = malloc(sizeof(app_files_template_context_t));
        strncpy(entry->key, key, sizeof(entry->key));
        HASH_ADD_STR(app_files_template_context, key, entry);
    }
    strncpy(entry->value, value, sizeof(entry->value));
    return ESP_OK;
}

const char *app_files_template_context_get(const char *key) {
    app_files_template_context_t *entry;
    HASH_FIND_STR(app_files_template_context, key, entry);
    return entry ? entry->value : NULL;
}

void app_files_template_context_clear() {
    app_files_template_context_t *current, *tmp;
    HASH_ITER(hh, app_files_template_context, current, tmp) {
        HASH_DEL(app_files_template_context, current);
        free(current);
    }
}

// Generic template rendering function
char *app_files_render_template(const char *template_buf, size_t template_len, const app_files_template_context_t *ctx, size_t *out_len) {
    if (!template_buf || !ctx || !out_len) {
        ESP_LOGE(TAG, "Invalid arguments to render_template");
        return NULL;
    }

    char *rendered_buf = malloc(template_len + 1);
    if (!rendered_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for rendered template");
        return NULL;
    }

    // Copy the original template buffer to the rendered buffer
    memcpy(rendered_buf, template_buf, template_len);
    rendered_buf[template_len] = '\0';

    // Replace placeholders with values from the context
    for (app_files_template_context_t *entry = ctx; entry != NULL; entry = entry->hh.next) {
        char *pos = strstr(rendered_buf, entry->key);
        if (pos) {
            size_t key_len = strlen(entry->key);
            size_t value_len = strlen(entry->value);
            size_t new_len = template_len - key_len + value_len;

            char *new_buf = realloc(rendered_buf, new_len + 1);
            if (!new_buf) {
                free(rendered_buf);
                ESP_LOGE(TAG, "Failed to allocate memory for resized template");
                return NULL;
            }
            rendered_buf = new_buf;

            // Shift the rest of the string
            memmove(pos + value_len, pos + key_len, template_len - (pos - rendered_buf) - key_len + 1);
            memcpy(pos, entry->value, value_len);

            template_len = new_len;
        }
    }

    *out_len = template_len;
    return rendered_buf;
}
