/*
 * Left and Right Gimbal Inputs
 * WHowe <github.com/whowechina>
 * 
 */

#include "gimbal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

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

    if (offset < -deadzone) {
        offset += deadzone;
        offset = offset * 2047 / (center - min - deadzone); 
    } else if (offset > deadzone) {
        offset -= deadzone;
        offset = offset * 2047 / (max - center - deadzone);
    } else {
        offset = 0;
    }

    offset *= 2;

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
    { true, true },
    { false, true },
};

uint16_t gimbal_raw(gimbal_axis_t axis)
{
    if (axis >= 4) {
        return 0;
    }

    gpio_put(AXIS_MUX_PIN_A, axis_mux[axis].a);
    gpio_put(AXIS_MUX_PIN_B, axis_mux[axis].b);

    const int sample_count = 4;
    uint32_t sum = 0;
    for (int i = 0; i < sample_count; i++) {
        sleep_us(10);
        sum += adc_read();
    }

    return sum / sample_count;
}
