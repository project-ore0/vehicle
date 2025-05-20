#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <stddef.h>

/*--- Available motors ---*/
typedef enum __attribute__((packed))
{
    MSG_MOTOR_1 = 0,
    MSG_MOTOR_2 = 1,
    MSG__MOTOR_MAX, /* for bounds checking */
} msg_motor_t;

/*––– Motor state enum –––*/
typedef enum __attribute__((packed))
{
    MSG_MOTOR_IDLE = 0,
    MSG_MOTOR_FORWARD = 1,
    MSG_MOTOR_BACKWARD = 2,
    MSG_MOTOR_BRAKE = 3,
    MSG__MOTOR_STATE_MAX, /* for bounds checking */
} msg_motor_state_t;

/*––– Message IDs –––*/
typedef enum __attribute__((packed))
{
    MSG_UNKNOWN = 0,
    MSG_MOTOR_STATE = 1,
    MSG_TELEMETRY = 2,
    MSG_CAMERA_CONTROL = 3,
    MSG_CAMERA_CHUNK = 4,
    MSG_MOTOR_CONTROL = 5,
    MSG_MOVE_CONTROL = 6,
    MSG_BATTERY_LEVEL = 7,
    MSG_DISTANCE_READING = 8,
    MSG__MSG_MAX, /* for bounds checking */
} msg_id_t;

/*––– Payload structs –––*/
#pragma pack(push, 1)
typedef struct
{
    msg_motor_t motor;
    msg_motor_state_t state;
} msg_payload_motor_state_t;

typedef struct
{
    msg_motor_state_t motor1;
    msg_motor_state_t motor2;
    uint8_t battery;  /* 0–255 */
    uint8_t distance; /* cm, 0–255 */
    uint8_t camera_fps; /* camera frames per second */
} msg_payload_telemetry_t;

typedef struct
{
    uint8_t on; /* 0 = off, 1 = on */
} msg_payload_camera_control_t;

typedef struct
{
    msg_motor_state_t motor1;
    msg_motor_state_t motor2;
} msg_payload_motor_control_t;

/* move_control is just one of eight commands, fits in one byte */
typedef enum __attribute__((packed))
{
    MSG_MOVE_M1_IDLE = 0,
    MSG_MOVE_M1_FWD = 1,
    MSG_MOVE_M1_BCK = 2,
    MSG_MOVE_M1_BRK = 3,
    MSG_MOVE_M2_IDLE = 4,
    MSG_MOVE_M2_FWD = 5,
    MSG_MOVE_M2_BCK = 6,
    MSG_MOVE_M2_BRK = 7,
    MSG__MOVE_MAX, /* for bounds checking */
} msg_move_cmd_t;

/* battery level */
typedef struct
{
    uint8_t level; /* 0–255 */
} msg_payload_battery_t;

/* distance reading */
typedef struct
{
    uint8_t forward; /* cm, 0–255 */
} msg_payload_distance_t;

/* move control command */
typedef struct
{
    msg_move_cmd_t cmd;
} msg_payload_move_control_t;
#pragma pack(pop)

/*––– Common message header + union –––*/
#pragma pack(push, 1)
typedef struct
{
    uint8_t msg_id;  /* one of msg_id_t */
    uint16_t length; /* total length of payload in bytes */
    union
    {
        msg_payload_motor_state_t mstate;
        msg_payload_telemetry_t telem;
        msg_payload_camera_control_t camctrl;
        msg_payload_motor_control_t mctrl;
        msg_payload_move_control_t mvctrl;
        msg_payload_battery_t battery;
        msg_payload_distance_t distance;
    } body;
} msg_t;
#pragma pack(pop)

inline msg_id_t msg_get_type(const uint8_t *buf, size_t len)
{
    if (!buf || len < 1)
        return MSG_UNKNOWN;
    const msg_id_t msg_id = (msg_id_t)buf[0];
    if(msg_id >= MSG__MSG_MAX)
        return MSG_UNKNOWN;
    return msg_id;
}

uint16_t msg_make_telemetry(uint8_t *buf, msg_motor_state_t m1, msg_motor_state_t m2, uint8_t battery, uint8_t dist, uint8_t camera_fps);
uint16_t msg_make_camera_control(uint8_t *buf, uint8_t on);
uint16_t msg_make_camera_frame(uint8_t *buf, uint8_t *data, uint16_t len);
uint16_t msg_make_motor_state(uint8_t *buf, msg_motor_t motor, msg_motor_state_t state);
uint16_t msg_make_motor_control(uint8_t *buf, msg_motor_state_t m1, msg_motor_state_t m2);
uint16_t msg_make_move_control(uint8_t *buf, msg_move_cmd_t cmd);
uint16_t msg_make_battery_level(uint8_t *buf, uint8_t percentage);
uint16_t msg_make_distance_reading(uint8_t *buf, uint8_t distance);

size_t msg_parse_telemetry(const uint8_t *buf, msg_motor_state_t *m1, msg_motor_state_t *m2, uint8_t *battery, uint8_t *dist, uint8_t *camera_fps);
size_t msg_parse_camera_control(const uint8_t *buf, uint8_t *on);
size_t msg_parse_motor_state(const uint8_t *buf, msg_motor_t *motor, msg_motor_state_t *state);
size_t msg_parse_motor_control(const uint8_t *buf, msg_motor_state_t *m1, msg_motor_state_t *m2);
size_t msg_parse_move_control(const uint8_t *buf, msg_move_cmd_t *cmd);
size_t msg_parse_camera(const uint8_t *buf, const uint8_t **data, uint16_t *data_len);
size_t msg_parse_battery_level(const uint8_t *buf, uint8_t *percentage);
size_t msg_parse_distance_reading(const uint8_t *buf, uint8_t *distance);

#endif // MESSAGES_H