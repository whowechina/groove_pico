/*
 * Left and Right Gimbal Inputs
 * WHowe <github.com/whowechina>
 * 
 */

#include "gimbal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "config.h"
#include "board_defs.h"

void gimbal_init()
{
    gpio_init(AXIS_MUX_PIN_A);
    gpio_set_dir(AXIS_MUX_PIN_A, GPIO_OUT);

    gpio_init(AXIS_MUX_PIN_B);
    gpio_set_dir(AXIS_MUX_PIN_B, GPIO_OUT);

    adc_init();
    adc_gpio_init(26 + ADC_CHANNEL);
    adc_select_input(ADC_CHANNEL);
}

uint16_t gimbal_read(gimbal_axis_t axis)
{
    if (axis >= 4) {
        return 0;
    }

    uint16_t raw = gimbal_raw(axis);
    const uint16_t min = groove_cfg->axis[axis].min;
    const uint16_t max = groove_cfg->axis[axis].max;
    const uint16_t center = groove_cfg->axis[axis].center;
    const uint8_t deadzone = groove_cfg->axis[axis].deadzone;

    if (raw < min) {
        raw = min;
    } else if (raw > max) {
        raw = max;
    }

    int offset = raw - center;
    int lower_deadzone = deadzone * (center - min) / 2 / 100;
    int upper_deadzone = deadzone * (max - center) / 2 / 100;

    if (offset < -lower_deadzone) {
        offset += lower_deadzone;
        offset = offset * 2047 / (center - min - lower_deadzone);
    } else if (offset > upper_deadzone) {
        offset -= upper_deadzone;
        offset = offset * 2047 / (max - center - upper_deadzone);
    } else {
        offset = 0;
    }

    if (groove_cfg->axis[axis].invert) {
        offset = -offset;
    }

    return offset + 2048;
}

static const struct {
    bool a;
    bool b;
} axis_mux[4] = {
    { false, false },
    { true, false },
    { false, true },
    { true, true },
};

uint16_t gimbal_raw(gimbal_axis_t axis)
{
    if (axis >= 4) {
        return 0;
    }

    gpio_put(AXIS_MUX_PIN_A, axis_mux[axis].a);
    gpio_put(AXIS_MUX_PIN_B, axis_mux[axis].b);

    static uint16_t last_read[4] = { 2048, 2048, 2048, 2048 };
    sleep_us(5);

    const uint16_t rate_limit = 10;
    uint16_t val = adc_read();
    if (val > last_read[axis] + rate_limit) {
        last_read[axis] += rate_limit;
    } else if (val < last_read[axis] - rate_limit) {
        last_read[axis] -= rate_limit;
    } else {
        last_read[axis] = val;
    }

    return last_read[axis];
}

int gimbal_get_dir(int gimbal)
{
    if (gimbal >= 2) {
        return -1;
    }

    int x = gimbal_read(gimbal ? GIMBAL_RIGHT_X : GIMBAL_LEFT_X) - 2048;
    int y = 2048 - gimbal_read(gimbal ? GIMBAL_RIGHT_Y : GIMBAL_LEFT_Y);

    int radius2 = x * x + y * y;
    if (radius2 < 2047 * 2047 * 7 / 8) {
        return -1;
    }

    int angle = (atan2(y, x) + M_PI * 13 / 8) * 4 / M_PI;
    int dir = angle % 8;

    return dir;
}
