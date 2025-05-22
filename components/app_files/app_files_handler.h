#ifndef APP_FILES_HANDLER_H
#define APP_FILES_HANDLER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "uthash.h"

/**
 * @brief Represents a key-value pair for templating
 * 
 * The key is limited to 32 characters and the value is limited to 224 characters.
 */
typedef struct {
    char key[32];    /**< The key to search for in templates */
    char value[224]; /**< The value to replace the key with */
    UT_hash_handle hh; /**< Makes this structure hashable */
} app_files_template_context_t;


/**
 * @brief HTTP request handler for serving static files
 * 
 * This function handles HTTP requests for static files. If the requested file is an HTML file
 * and a template context has been set, the file will be treated as a template and the placeholders
 * will be replaced with values from the template context.
 * 
 * @param req The HTTP request to handle
 * @return esp_err_t ESP_OK on success, or an error code on failure
 */
esp_err_t app_files_handler(httpd_req_t *req);


esp_err_t app_files_template_context_set(const char *key, const char *value);

const char *app_files_template_context_get(const char *key);

void app_files_template_context_clear();


/**
 * @brief Render a template buffer with key/value pairs
 * 
 * Supports multiple template patterns:
 * - {{ key }}    (Mustache/Jinja2 style)
 * - {key}        (Simple style)
 * - <!--key-->   (HTML comment style)
 * - $(key)       (Shell style)
 * 
 * @param template_buf The template buffer to render
 * @param template_len The length of the template buffer
 * @param ctx The template context containing key/value pairs
 * @param out_len Pointer to store the length of the rendered buffer
 * @return char* A newly allocated buffer containing the rendered template, or NULL on error
 */
char *app_files_render_template(const char *template_buf, size_t template_len, const app_files_template_context_t *ctx, size_t *out_len);

#endif // APP_FILES_HANDLER_H
