#ifndef APP_FILES_HANDLER_H
#define APP_FILES_HANDLER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Represents a key-value pair for templating
 * 
 * The key is limited to 32 characters and the value is limited to 224 characters.
 */
typedef struct {
    char key[32];    /**< The key to search for in templates */
    char value[224]; /**< The value to replace the key with */
} app_files_template_kv_t;

/**
 * @brief Represents a context for templating, a list of key-value pairs
 * 
 * This structure is used to pass a list of key-value pairs to the template rendering function.
 */
typedef struct {
    app_files_template_kv_t *kv_pairs; /**< Array of key-value pairs */
    size_t kv_count;                   /**< Number of key-value pairs in the array */
} app_files_template_context_t;


/**
 * @brief Set the template context for HTML rendering
 * 
 * This function sets the template context that will be used when rendering HTML files.
 * The context is a list of key/value pairs that will be used to replace placeholders in HTML files.
 * The function makes a deep copy of the context, so the caller can free the context after calling this function.
 * 
 * @param ctx The template context to set, or NULL to clear the context
 * @return esp_err_t ESP_OK on success, or an error code on failure
 */
esp_err_t app_files_set_template_context(const app_files_template_context_t *ctx);

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
