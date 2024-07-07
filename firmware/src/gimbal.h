/*
 * Left and Right Gimbal Inputs
 * WHowe <github.com/whowechina>
 */

#ifndef GIMBAL_H
#define GIMBAL_H

#include <stdint.h>
#include <stdbool.h>

void gimbal_init();

typedef enum {
    GIMBAL_LEFT_X = 0,
    GIMBAL_LEFT_Y,
    GIMBAL_RIGHT_X,
    GIMBAL_RIGHT_Y,
} gimbal_axis_t;

uint16_t gimbal_read(gimbal_axis_t axis);
uint16_t gimbal_raw(gimbal_axis_t axis);

#endif
