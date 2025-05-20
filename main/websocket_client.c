#include "websocket_client.h"
#include "config.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "camera.h"
#include "messages.h"
#include "motor.h"
#include <string.h>

static const char *TAG = "WS_CLIENT";
static esp_websocket_client_handle_t client = NULL;
static TaskHandle_t cam_task = NULL;
static TaskHandle_t cmd_task = NULL;
static bool camera_enabled = true;
static SemaphoreHandle_t client_mutex = NULL;
static QueueHandle_t ws_ctrl_queue = NULL;

typedef enum
{
    WS_CTRL_RECONNECT
} ws_ctrl_msg_t;

// Simulated sensor values (replace with real sensor readings)
static uint8_t distance_reading = 60;
static uint8_t battery_level = 100;

// Forward declarations
static void camera_frame_task(void *arg);
static void command_task(void *arg);
static void process_command(const uint8_t *data, size_t len);

static void websocket_client_reinit_from_nvm(void);

// WebSocket event handler
void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_DATA:
        if (data->data_ptr && data->data_len > 0)
        {
            // Process binary command data
            process_command((const uint8_t *)data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_ERROR:
    {
        ESP_LOGW(TAG, "WebSocket disconnected or error, reloading ws_uri and reconnecting");
        // Post reconnect message to control queue
        if (ws_ctrl_queue)
        {
            ws_ctrl_msg_t msg = WS_CTRL_RECONNECT;
            if (xQueueSend(ws_ctrl_queue, &msg, 0) != pdTRUE) {
                ESP_LOGE(TAG, "Failed to queue WS_CTRL_RECONNECT (queue full)");
            }
        }
        break;
    }
    default:
        break;
    }
}

// Helper to (re)initialize and start websocket client from NVM
static void websocket_client_reinit_from_nvm(void)
{
    char ws_uri[128] = {0};
    if (load_ws_uri(ws_uri, sizeof(ws_uri)) == ESP_OK && strlen(ws_uri) > 0)
    {
        esp_websocket_client_config_t ws_cfg = {
            .uri = ws_uri,
            .buffer_size = 4096,
            .disable_auto_reconnect = false,
            .reconnect_timeout_ms = 10000,
            .network_timeout_ms = 10000,
            .use_global_ca_store = true,
            .transport = WEBSOCKET_TRANSPORT_OVER_SSL,
            .pingpong_timeout_sec = 30,
        };
        client = esp_websocket_client_init(&ws_cfg);
        if (client)
        {
            esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
            esp_err_t start_err = esp_websocket_client_start(client);
            if (start_err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to start websocket client: %s", esp_err_to_name(start_err));
                esp_websocket_client_destroy(client);
                client = NULL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to init websocket client");
        }
    }
    else
    {
        ESP_LOGE(TAG, "No valid ws_uri in NVM, not reconnecting");
    }
}

void websocket_client_start(void)
{
    if (client)
        return;

    // Initialize camera
    esp_err_t camera_err = camera_init();
    if (camera_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize camera: %s", esp_err_to_name(camera_err));
        camera_enabled = false;
    }

    // Create mutex for client access
    client_mutex = xSemaphoreCreateMutex();
    if (!client_mutex)
    {
        ESP_LOGE(TAG, "Failed to create client mutex");
        return;
    }
    // Create control queue for websocket events
    ws_ctrl_queue = xQueueCreate(2, sizeof(ws_ctrl_msg_t));
    if (!ws_ctrl_queue)
    {
        ESP_LOGE(TAG, "Failed to create websocket control queue");
        return;
    }
    websocket_client_reinit_from_nvm();
    if (!client)
        return;

    // Create tasks for camera frames and command handling
    // Camera task has lower priority (2)
    xTaskCreatePinnedToCore(camera_frame_task, "ws_cam", 8192, NULL, 2, &cam_task, 1);
    // Command task has higher priority (5)
    xTaskCreatePinnedToCore(command_task, "ws_cmd", 4096, NULL, 5, &cmd_task, 1);
}

void websocket_client_stop(void)
{
    if (client)
    {
        // Take mutex before stopping client
        if (client_mutex)
        {
            xSemaphoreTake(client_mutex, portMAX_DELAY);
        }
        esp_websocket_client_stop(client);
        esp_websocket_client_destroy(client);
        client = NULL;
        if (client_mutex)
        {
            xSemaphoreGive(client_mutex);
        }
    }

    // Stop camera
    // camera_stop();

    // Delete tasks
    if (cam_task)
    {
        vTaskDelete(cam_task);
        cam_task = NULL;
    }
    if (cmd_task)
    {
        vTaskDelete(cmd_task);
        cmd_task = NULL;
    }

    // Delete mutex
    if (client_mutex)
    {
        vSemaphoreDelete(client_mutex);
        client_mutex = NULL;
    }
    // Delete control queue
    if (ws_ctrl_queue)
    {
        vQueueDelete(ws_ctrl_queue);
        ws_ctrl_queue = NULL;
    }
}

// Camera frame task - runs at low priority
static volatile uint8_t g_camera_fps = 0; // Global for current camera FPS

static void camera_frame_task(void *arg)
{
    const TickType_t delay_between_frames = pdMS_TO_TICKS(100); // 10 FPS
    TickType_t last_fps_log = xTaskGetTickCount();
    int frame_count = 0;
    int camera_frame_count = 0;
    while (1)
    {
        // Only send frames if camera is enabled and client is connected
        if (camera_enabled && client && esp_websocket_client_is_connected(client))
        {
            // Capture frame from camera
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb)
            {
                camera_frame_count++;
                // Prepare frame header: [msg_id][uint16_t payload_size][image_data...]
                uint16_t payload_len = fb->len;
                uint8_t *frame_data = malloc(1 + 2 + payload_len);
                if (frame_data)
                {
                    size_t total_size = msg_make_camera_frame(frame_data, fb->buf, payload_len);
                    if (xSemaphoreTake(client_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
                    {
                        esp_websocket_client_send_bin(client, (const char *)frame_data, total_size, portMAX_DELAY);
                        xSemaphoreGive(client_mutex);
                        frame_count++;
                    }
                    free(frame_data);
                }
                esp_camera_fb_return(fb);
            }
        }
        // Log FPS every 5 seconds
        TickType_t now = xTaskGetTickCount();
        if ((now - last_fps_log) >= pdMS_TO_TICKS(5000))
        {
            g_camera_fps = camera_frame_count / 5; // update global FPS
            ESP_LOGI(TAG, "Camera FPS: %d, WebSocket FPS: %d", g_camera_fps, frame_count/5);
            frame_count = 0;
            camera_frame_count = 0;
            last_fps_log = now;
        }
        // Delay before next frame
        vTaskDelay(delay_between_frames);
    }
}

// Command/telemetry handler task - runs at high priority
static void command_task(void *arg)
{
    const TickType_t telemetry_interval = pdMS_TO_TICKS(1000); // Send telemetry every 1 second
    const TickType_t reconnect_watchdog_interval = pdMS_TO_TICKS(10000); // Watchdog every 10 seconds
    TickType_t last_telemetry_time = xTaskGetTickCount();
    TickType_t last_reconnect_watchdog = xTaskGetTickCount();
    ws_ctrl_msg_t ctrl_msg;
    while (1)
    {
        // Handle websocket control messages
        if (ws_ctrl_queue && xQueueReceive(ws_ctrl_queue, &ctrl_msg, 0) == pdTRUE)
        {
            if (ctrl_msg == WS_CTRL_RECONNECT)
            {
                if (client_mutex)
                    xSemaphoreTake(client_mutex, portMAX_DELAY);
                if (client)
                {
                    esp_websocket_client_stop(client);
                    esp_websocket_client_destroy(client);
                    client = NULL;
                }
                if (client_mutex)
                    xSemaphoreGive(client_mutex);
                websocket_client_reinit_from_nvm();
            }
        }
        // Check if it's time to send telemetry
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_telemetry_time) >= telemetry_interval)
        {
            send_status_response();
            // Update last telemetry time
            last_telemetry_time = current_time;
        }
        // Watchdog: periodically check and force reconnect if not connected
        if ((current_time - last_reconnect_watchdog) >= reconnect_watchdog_interval)
        {
            last_reconnect_watchdog = current_time;
            if (!client || !esp_websocket_client_is_connected(client)) {
                ESP_LOGW(TAG, "Watchdog: WebSocket not connected, forcing reconnect");
                if (ws_ctrl_queue) {
                    ws_ctrl_msg_t msg = WS_CTRL_RECONNECT;
                    // Use overwrite to guarantee delivery
                    xQueueOverwrite(ws_ctrl_queue, &msg);
                }
            }
        }
        // Short delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Set camera state
void set_camera_state(bool on)
{
    if (on && !camera_enabled)
    {
        // Turn camera on
        ESP_LOGI(TAG, "Turning camera ON");
        esp_err_t err = camera_init();
        if (err == ESP_OK)
        {
            camera_enabled = true;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize camera: %s", esp_err_to_name(err));
        }
    }
    else if (!on && camera_enabled)
    {
        // Turn camera off
        ESP_LOGI(TAG, "Turning camera OFF");
        camera_stop();
        camera_enabled = false;
    }
}

// Send distance reading
void send_distance_reading(uint8_t distance)
{
    if (!client || !esp_websocket_client_is_connected(client))
        return;

    uint8_t msg_buf[sizeof(msg_t)];
    // Clamp and convert float to uint8_t (0-255 cm)
    uint16_t len = msg_make_distance_reading(msg_buf, distance);

    if (xSemaphoreTake(client_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        esp_websocket_client_send_bin(client, (const char *)msg_buf, len, portMAX_DELAY);
        xSemaphoreGive(client_mutex);
    }
}

// Send battery level
void send_battery_level(uint8_t level)
{
    if (!client || !esp_websocket_client_is_connected(client))
        return;

    uint8_t msg_buf[sizeof(msg_t)];
    uint16_t len = msg_make_battery_level(msg_buf, level);

    if (xSemaphoreTake(client_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        esp_websocket_client_send_bin(client, (const char *)msg_buf, len, portMAX_DELAY);
        xSemaphoreGive(client_mutex);
    }
}

// Send status response (telemetry)
void send_status_response(void)
{
    if (!client || !esp_websocket_client_is_connected(client))
        return;

    uint8_t msg_buf[sizeof(msg_t)];
    msg_motor_state_t motor1, motor2;
    motor_get(MSG_MOTOR_1, &motor1);
    motor_get(MSG_MOTOR_2, &motor2);

    // Simulate battery drain (replace with real battery monitoring)
    if (battery_level > 0)
    {
        battery_level--;
    }

    // Simulate distance reading changes (replace with real sensor)
    distance_reading = (uint8_t)(rand() % 256);
    
    uint16_t len = msg_make_telemetry(msg_buf, motor1, motor2, battery_level, (uint8_t)distance_reading, g_camera_fps);

    if (xSemaphoreTake(client_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        esp_websocket_client_send_bin(client, (const char *)msg_buf, len, portMAX_DELAY);
        xSemaphoreGive(client_mutex);
    }
}

// Process incoming command
static void process_command(const uint8_t *data, size_t len)
{
    if (!data || len < 1)
        return;

    msg_id_t msg_type = msg_get_type(data, len);
    switch (msg_type)
    {
    case MSG_CAMERA_CONTROL:
    {
        uint8_t camera_on;
        if (msg_parse_camera_control(data, &camera_on) > 0)
        {
            ESP_LOGI(TAG, "Camera control: %d", camera_on);
            set_camera_state(camera_on);
        }
        break;
    }
    case MSG_MOTOR_STATE:
    {
        msg_motor_t motor;
        msg_motor_state_t state;
        if (msg_parse_motor_state(data, &motor, &state) > 0)
        {
            ESP_LOGI(TAG, "Motor %d state: %d", motor, state);
            motor_set(motor, state);
        }
        break;
    }
    case MSG_MOTOR_CONTROL:
    {
        msg_motor_state_t m1, m2;
        if (msg_parse_motor_control(data, &m1, &m2) > 0)
        {
            ESP_LOGI(TAG, "Motor control: m1=%d, m2=%d", m1, m2);
            motor_set(MSG_MOTOR_1, m1);
            motor_set(MSG_MOTOR_2, m2);
        }
        break;
    }
    case MSG_MOVE_CONTROL:
    {
        msg_move_cmd_t cmd;
        if (msg_parse_move_control(data, &cmd) > 0)
        {
            ESP_LOGI(TAG, "Received move command: %d", cmd);
            switch (cmd)
            {
            case MSG_MOVE_M1_IDLE:
                motor_set(MSG_MOTOR_1, MSG_MOTOR_IDLE);
                break;
            case MSG_MOVE_M1_FWD:
                motor_set(MSG_MOTOR_1, MSG_MOTOR_FORWARD);
                break;
            case MSG_MOVE_M1_BCK:
                motor_set(MSG_MOTOR_1, MSG_MOTOR_BACKWARD);
                break;
            case MSG_MOVE_M1_BRK:
                motor_set(MSG_MOTOR_1, MSG_MOTOR_BRAKE);
                break;
            case MSG_MOVE_M2_IDLE:
                motor_set(MSG_MOTOR_2, MSG_MOTOR_IDLE);
                break;
            case MSG_MOVE_M2_FWD:
                motor_set(MSG_MOTOR_2, MSG_MOTOR_FORWARD);
                break;
            case MSG_MOVE_M2_BCK:
                motor_set(MSG_MOTOR_2, MSG_MOTOR_BACKWARD);
                break;
            case MSG_MOVE_M2_BRK:
                motor_set(MSG_MOTOR_2, MSG_MOTOR_BRAKE);
                break;
            default:
                ESP_LOGW(TAG, "Unknown move command: %d", cmd);
                break;
            }
        }
        break;
    }
    case MSG_TELEMETRY:
        ESP_LOGD(TAG, "Ignoring telemetry message");
        break;
    case MSG_CAMERA_CHUNK:
        ESP_LOGD(TAG, "Ignoring camera chunk message");
        break;
    default:
        ESP_LOGW(TAG, "Unknown or unsupported msg_type: 0x%02x", msg_type);
        break;
    }
}
