/*
 * Haptic Feedback
 * WHowe <github.com/whowechina>
 * 
 */

#include "haptics.h"

#include <stdint.h>
#include <stdbool.h>

#include "hardware/gpio.h"

#include "config.h"
#include "board_defs.h"

static const uint8_t haptics_gpio[2] = HAPTICS_DEF;

void haptics_init()
{
    for (int i = 0; i < 2; i++)
    {
        uint8_t gpio = haptics_gpio[i];
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_put(gpio, false);
    }
}

void haptics_set(int id, bool on)
{
    if (!groove_cfg->haptics.enabled) {
        gpio_put(haptics_gpio[id], false);
        return;
    }

    if (id >= 2) {
        return;
    }

    gpio_put(haptics_gpio[id], on);
}
