
#include "driver/gpio.h"

#include "motor.h"
#include "config.h"

gpio_num_t _pin1a, _pin1b, _pin2a, _pin2b;
msg_motor_state_t _motor1, _motor2;

esp_err_t motor_read_config(void)
{
    uint8_t pin1a, pin1b, pin2a, pin2b;
    esp_err_t err = load_motor_config(&pin1a, &pin1b, &pin2a, &pin2b);
    if (err != ESP_OK)
    {
        // If loading fails, use default values
        _pin1a = CONFIG_DEFAULT_MOTOR1A;
        _pin1b = CONFIG_DEFAULT_MOTOR1B;
        _pin2a = CONFIG_DEFAULT_MOTOR2A;
        _pin2b = CONFIG_DEFAULT_MOTOR2B;
        return err;
    }
    _pin1a = (gpio_num_t)pin1a;
    _pin1b = (gpio_num_t)pin1b;
    _pin2a = (gpio_num_t)pin2a;
    _pin2b = (gpio_num_t)pin2b;
    return ESP_OK;
}

esp_err_t motor_init(void)
{
    esp_err_t err = motor_read_config();
    if (err != ESP_OK)
    {
        return err;
    }
    // Initialize GPIO pins for motor control
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << _pin1a) | (1ULL << _pin1b) | (1ULL << _pin2a) | (1ULL << _pin2b),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        return err;
    }
    // Set all pins to low initially
    err = gpio_set_level(_pin1a, 0);
    if (err == ESP_OK)
    {
        err = gpio_set_level(_pin1b, 0);
    }
    if (err == ESP_OK)
    {
        err = gpio_set_level(_pin2a, 0);
    }
    if (err == ESP_OK)
    {
        err = gpio_set_level(_pin2b, 0);
    }
    return err;
}

esp_err_t motor_set(msg_motor_t motor, msg_motor_state_t state)
{
    esp_err_t err = ESP_OK;
    gpio_port_t pina, pinb;
    switch (motor)
    {
    case MSG_MOTOR_1:
        pina = _pin1a;
        pinb = _pin1b;
        _motor1 = state;
        break;
    case MSG_MOTOR_2:
        pina = _pin2a;
        pinb = _pin2b;
        _motor2 = state;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    switch (state)
    {
    case MSG_MOTOR_IDLE:
        err = gpio_set_level(pina, 0);
        if (err == ESP_OK)
        {
            err = gpio_set_level(pinb, 0);
        }
        break;
    case MSG_MOTOR_FORWARD:
        err = gpio_set_level(pina, 1);
        if (err == ESP_OK)
        {
            err = gpio_set_level(pinb, 0);
        }
        break;
    case MSG_MOTOR_BACKWARD:
        err = gpio_set_level(pina, 0);
        if (err == ESP_OK)
        {
            err = gpio_set_level(pinb, 1);
        }
        break;
    case MSG_MOTOR_BRAKE:
        err = gpio_set_level(pina, 1);
        if (err == ESP_OK)
        {
            err = gpio_set_level(pinb, 1);
        }
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return err;
}

esp_err_t motor_get(msg_motor_t motor, msg_motor_state_t *state)
{
    if (!state)
    {
        return ESP_ERR_INVALID_ARG;
    }
    switch (motor)
    {
    case MSG_MOTOR_1:
        *state = _motor1;
        break;
    case MSG_MOTOR_2:
        *state = _motor2;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t motor_forward(void)
{
    esp_err_t err = motor_set(MSG_MOTOR_1, MSG_MOTOR_FORWARD);
    if (err == ESP_OK)
    {
        err = motor_set(MSG_MOTOR_2, MSG_MOTOR_FORWARD);
    }
    return err;
}

esp_err_t motor_backward(void)
{
    esp_err_t err = motor_set(MSG_MOTOR_1, MSG_MOTOR_BACKWARD);
    if (err == ESP_OK)
    {
        err = motor_set(MSG_MOTOR_2, MSG_MOTOR_BACKWARD);
    }
    return err;
}

esp_err_t motor_brake(void)
{
    esp_err_t err = motor_set(MSG_MOTOR_1, MSG_MOTOR_BRAKE);
    if (err == ESP_OK)
    {
        err = motor_set(MSG_MOTOR_2, MSG_MOTOR_BRAKE);
    }
    return err;
}

esp_err_t motor_off(void)
{
    esp_err_t err = motor_set(MSG_MOTOR_1, MSG_MOTOR_IDLE);
    if (err == ESP_OK)
    {
        err = motor_set(MSG_MOTOR_2, MSG_MOTOR_IDLE);
    }
    return err;
}
