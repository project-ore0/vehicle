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
            if (app_files_template_context != NULL && 
                (strcmp(app_files[i].content_type, "text/html") == 0 ||
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

esp_err_t app_files_set_template_context(const app_files_template_context_t *ctx) {
    if (app_files_template_context) {
        free(app_files_template_context->kv_pairs);
        free(app_files_template_context);
        app_files_template_context = NULL;
    }
    
    if (!ctx) {
        ESP_LOGI(TAG, "Template context cleared");
        return ESP_OK;
    }
    
    if (!ctx->kv_pairs || ctx->kv_count == 0) {
        ESP_LOGE(TAG, "Invalid template context");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create a deep copy of the context
    app_files_template_context = malloc(sizeof(app_files_template_context_t));
    if (!app_files_template_context) {
        ESP_LOGE(TAG, "Failed to allocate memory for template context");
        return ESP_ERR_NO_MEM;
    }
    
    app_files_template_context->kv_count = ctx->kv_count;
    app_files_template_context->kv_pairs = malloc(ctx->kv_count * sizeof(app_files_template_kv_t));
    if (!app_files_template_context->kv_pairs) {
        free(app_files_template_context);
        app_files_template_context = NULL;
        ESP_LOGE(TAG, "Failed to allocate memory for template key-value pairs");
        return ESP_ERR_NO_MEM;
    }
    
    // Copy each key-value pair
    for (size_t i = 0; i < ctx->kv_count; i++) {
        strncpy(app_files_template_context->kv_pairs[i].key, ctx->kv_pairs[i].key, sizeof(app_files_template_context->kv_pairs[i].key) - 1);
        app_files_template_context->kv_pairs[i].key[sizeof(app_files_template_context->kv_pairs[i].key) - 1] = '\0';
        
        strncpy(app_files_template_context->kv_pairs[i].value, ctx->kv_pairs[i].value, sizeof(app_files_template_context->kv_pairs[i].value) - 1);
        app_files_template_context->kv_pairs[i].value[sizeof(app_files_template_context->kv_pairs[i].value) - 1] = '\0';
    }
    
    ESP_LOGI(TAG, "Template context set with %d key-value pairs", ctx->kv_count);
    return ESP_OK;
}

// Generic template rendering function
char *app_files_render_template(const char *template_buf, size_t template_len, const app_files_template_context_t *ctx, size_t *out_len) {
    if (!template_buf || !ctx || !ctx->kv_pairs || ctx->kv_count == 0) {
        // Just copy the template as-is
        char *out = malloc(template_len + 1);
        if (!out) return NULL;
        memcpy(out, template_buf, template_len);
        out[template_len] = '\0';
        if (out_len) *out_len = template_len;
        return out;
    }
    
    // Estimate output size: template_len + extra space for replacements
    // Each key might be replaced with a value that's longer
    size_t max_extra = ctx->kv_count * 256;
    size_t out_buf_size = template_len + max_extra + 1;
    char *out = malloc(out_buf_size);
    if (!out) return NULL;
    
    const char *src = template_buf;
    char *dst = out;
    size_t remain = out_buf_size - 1;
    
    // Define the template patterns we support
    const char *patterns[] = {
        "{{ %s }}",  // Mustache/Jinja2 style
        "{%s}",      // Simple style
        "<!--%s-->", // HTML comment style
        "$(%s)"      // Shell style
    };
    const int num_patterns = sizeof(patterns) / sizeof(patterns[0]);
    
    while (*src && remain > 0) {
        int replaced = 0;
        
        // Try each key with each pattern
        for (size_t i = 0; i < ctx->kv_count && !replaced; ++i) {
            for (int p = 0; p < num_patterns && !replaced; p++) {
                char keypat[64];
                snprintf(keypat, sizeof(keypat), patterns[p], ctx->kv_pairs[i].key);
                size_t klen = strlen(keypat);
                
                if (strncmp(src, keypat, klen) == 0) {
                    // Found a match, replace with the value
                    size_t vlen = strnlen(ctx->kv_pairs[i].value, sizeof(ctx->kv_pairs[i].value));
                    if (vlen > remain) vlen = remain;
                    
                    memcpy(dst, ctx->kv_pairs[i].value, vlen);
                    dst += vlen;
                    src += klen;
                    remain -= vlen;
                    replaced = 1;
                    
                    ESP_LOGI(TAG, "Replaced '%s' with '%s'", keypat, ctx->kv_pairs[i].value);
                }
            }
        }
        
        if (!replaced) {
            *dst++ = *src++;
            --remain;
        }
    }
    
    *dst = '\0';
    if (out_len) *out_len = dst - out;
    return out;
}
