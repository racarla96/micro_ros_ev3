#ifndef EV3_MOTOR_H
#define EV3_MOTOR_H

#include <stdbool.h>

#define EV3_MOTOR_PATH_MAX 64

typedef struct {
    char base_path[EV3_MOTOR_PATH_MAX];
    int  count_per_rot;
} ev3_motor_t;

/* port is an ev3dev output port address, e.g. "outB" or "outC". */
bool ev3_motor_open(const char * port, ev3_motor_t * motor);
void ev3_motor_set_duty_cycle(const ev3_motor_t * motor, int percent);
void ev3_motor_stop(const ev3_motor_t * motor);

/* Tacho counts per second, signed (direction follows motor polarity). */
int ev3_motor_get_speed(const ev3_motor_t * motor);

#endif
