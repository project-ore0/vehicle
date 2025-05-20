#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "messages.h"

/*––– serializing a telemetry packet –––*/
uint16_t msg_make_telemetry(uint8_t *buf,
                   msg_motor_state_t m1, msg_motor_state_t m2,
                   uint8_t battery, uint8_t dist, uint8_t camera_fps)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_TELEMETRY;
    m->length = sizeof(msg_payload_telemetry_t);
    m->body.telem.motor1   = m1;
    m->body.telem.motor2   = m2;
    m->body.telem.battery  = battery;
    m->body.telem.distance = dist;
    m->body.telem.camera_fps = camera_fps;
    /* total bytes to send = sizeof(msg_id) + sizeof(length) + payload */
    return 1 + 2 + m->length;
}

/*––– serializing a camera control packet –––*/
uint16_t msg_make_camera_control(uint8_t *buf, uint8_t on)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_CAMERA_CONTROL;
    m->length = sizeof(msg_payload_camera_control_t);
    m->body.camctrl.on = on;
    /* total bytes to send = sizeof(msg_id) + sizeof(length) + payload */
    return 1 + 2 + m->length;
}

uint16_t msg_make_camera_frame(uint8_t *buf, uint8_t *data, uint16_t len)
{
    // Format: [msg_id][uint16_t length][data...]
    buf[0] = MSG_CAMERA_CHUNK;
    buf[1] = len & 0xFF;
    buf[2] = (len >> 8) & 0xFF;
    memcpy(buf + 3, data, len);
    return 1 + 2 + len;
}

/*––– serializing a motor state packet –––*/
uint16_t msg_make_motor_state(uint8_t *buf, msg_motor_t motor, msg_motor_state_t state)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_MOTOR_STATE;
    m->length = sizeof(msg_payload_motor_state_t);
    m->body.mstate.motor = motor;
    m->body.mstate.state = state;
    return 1 + 2 + m->length;
}

/*––– serializing a motor control packet –––*/
uint16_t msg_make_motor_control(uint8_t *buf, msg_motor_state_t m1, msg_motor_state_t m2)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_MOTOR_CONTROL;
    m->length = sizeof(msg_payload_motor_control_t);
    m->body.mctrl.motor1 = m1;
    m->body.mctrl.motor2 = m2;
    return 1 + 2 + m->length;
}

/*––– serializing a move control packet –––*/
uint16_t msg_make_move_control(uint8_t *buf, msg_move_cmd_t cmd)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_MOVE_CONTROL;
    m->length = sizeof(msg_payload_move_control_t);
    m->body.mvctrl.cmd = cmd;
    return 1 + 2 + m->length;
}

/*––– battery level –––*/
uint16_t msg_make_battery_level(uint8_t *buf, uint8_t percentage)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_BATTERY_LEVEL;
    m->length = sizeof(msg_payload_move_control_t);
    m->body.telem.battery = percentage;
    return 1 + 2 + m->length;
}

/*––– distance reading –––*/
uint16_t msg_make_distance_reading(uint8_t *buf, uint8_t distance)
{
    msg_t *m = (msg_t*)buf;
    m->msg_id = MSG_DISTANCE_READING;
    m->length = sizeof(msg_payload_move_control_t);
    m->body.telem.distance = distance;
    return 1 + 2 + m->length;
}

/*––– parsing camera control packet –––*/
size_t msg_parse_camera_control(const uint8_t *buf, uint8_t *on)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_CAMERA_CONTROL || m->length != sizeof(msg_payload_camera_control_t))
        return -1;
    *on = m->body.camctrl.on;
    return sizeof(msg_t);
}


/*––– parsing a motor state packet –––*/
size_t msg_parse_motor_state(const uint8_t *buf, msg_motor_t *motor, msg_motor_state_t *state)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_MOTOR_STATE || m->length != sizeof(msg_payload_motor_state_t))
        return -1;
    *motor = m->body.mstate.motor;
    *state = m->body.mstate.state;
    return sizeof(msg_t);
}

/*––– parsing a telemetry packet –––*/
size_t msg_parse_telemetry(const uint8_t *buf, msg_motor_state_t *m1, msg_motor_state_t *m2, uint8_t *battery, uint8_t *dist, uint8_t *camera_fps)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_TELEMETRY || m->length != sizeof(msg_payload_telemetry_t))
        return -1;
    *m1 = m->body.telem.motor1;
    *m2 = m->body.telem.motor2;
    *battery = m->body.telem.battery;
    *dist = m->body.telem.distance;
    *camera_fps = m->body.telem.camera_fps;
    return sizeof(msg_t);
}

/*––– parsing a motor control packet –––*/
size_t msg_parse_motor_control(const uint8_t *buf, msg_motor_state_t *m1, msg_motor_state_t *m2)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_MOTOR_CONTROL || m->length != sizeof(msg_payload_motor_control_t))
        return -1;
    *m1 = m->body.mctrl.motor1;
    *m2 = m->body.mctrl.motor2;
    return sizeof(msg_t);
}

/*––– parsing a move control packet –––*/
size_t msg_parse_move_control(const uint8_t *buf, msg_move_cmd_t *cmd)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_MOVE_CONTROL || m->length != sizeof(msg_payload_move_control_t))
        return -1;
    *cmd = m->body.mvctrl.cmd;
    return sizeof(msg_t);
}

/*––– parsing a camera chunk packet –––*/
size_t msg_parse_camera(const uint8_t *buf, const uint8_t **data, uint16_t *data_len)
{
    if (!buf) return -1;
    if (buf[0] != MSG_CAMERA_CHUNK) return -1;
    uint16_t len = buf[1] | (buf[2] << 8);
    *data_len = len;
    *data = buf + 3;
    return 1 + 2 + len;
}

/*––– parsing a battery level packet –––*/
size_t msg_parse_battery_level(const uint8_t *buf, uint8_t *percentage)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_BATTERY_LEVEL || m->length != sizeof(msg_payload_battery_t))
        return -1;
    *percentage = m->body.battery.level;
    return sizeof(msg_t);
}

/*––– parsing a distance reading packet –––*/
size_t msg_parse_distance_reading(const uint8_t *buf, uint8_t *distance)
{
    const msg_t *m = (const msg_t*)buf;
    if (m->msg_id != MSG_DISTANCE_READING || m->length != sizeof(msg_payload_distance_t))
        return -1;
    *distance = m->body.distance.forward;
    return sizeof(msg_t);
}
