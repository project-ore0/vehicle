#ifndef MOTOR_H
#define MOTOR_H

#include "esp_err.h"

#include "messages.h"

esp_err_t motor_read_config(void);
esp_err_t motor_init(void);
esp_err_t motor_set(msg_motor_t motor, msg_motor_state_t state);
esp_err_t motor_get(msg_motor_t motor, msg_motor_state_t *state);
esp_err_t motor_forward(void);
esp_err_t motor_backward(void);
esp_err_t motor_brake(void);
esp_err_t motor_off(void);


#endif // MOTOR_H