/*
 * WS2812B Lights Control (Base + Left and Right Gimbals)
 * WHowe <github.com/whowechina>
 * 
 */

#include "light.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/pio.h"
#include "hardware/timer.h"

#include "ws2812.pio.h"

#include "board_defs.h"
#include "config.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static uint32_t buf_base[19]; // 8 + 3 + 8
static uint32_t buf_left[12];
static uint32_t buf_right[12];

#define _MAP_LED(x) _MAKE_MAPPER(x)
#define _MAKE_MAPPER(x) MAP_LED_##x
#define MAP_LED_RGB { c1 = r; c2 = g; c3 = b; }
#define MAP_LED_GRB { c1 = g; c2 = r; c3 = b; }

#define REMAP_BUTTON_RGB _MAP_LED(BUTTON_RGB_ORDER)
#define REMAP_TT_RGB _MAP_LED(TT_RGB_ORDER)

static inline uint32_t _rgb32(uint32_t c1, uint32_t c2, uint32_t c3, bool gamma_fix)
{
    if (gamma_fix) {
        c1 = ((c1 + 1) * (c1 + 1) - 1) >> 8;
        c2 = ((c2 + 1) * (c2 + 1) - 1) >> 8;
        c3 = ((c3 + 1) * (c3 + 1) - 1) >> 8;
    }
    
    return (c1 << 16) | (c2 << 8) | (c3 << 0);    
}

uint32_t rgb32(uint32_t r, uint32_t g, uint32_t b, bool gamma_fix)
{
#if BUTTON_RGB_ORDER == GRB
    return _rgb32(g, r, b, gamma_fix);
#else
    return _rgb32(r, g, b, gamma_fix);
#endif
}

uint32_t rgb32_from_hsv(uint8_t h, uint8_t s, uint8_t v)
{
    uint32_t region, remainder, p, q, t;

    if (s == 0) {
        return v << 16 | v << 8 | v;
    }

    region = h / 43;
    remainder = (h % 43) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            return v << 16 | t << 8 | p;
        case 1:
            return q << 16 | v << 8 | p;
        case 2:
            return p << 16 | v << 8 | t;
        case 3:
            return p << 16 | q << 8 | v;
        case 4:
            return t << 16 | p << 8 | v;
        default:
            return v << 16 | p << 8 | q;
    }
}

#define DRIVE_LED_PIO(pio, sm, buf) \
    for (int i = 0; i < ARRAY_SIZE(buf); i++) { \
        pio_sm_put_blocking(pio, sm, buf[i] << 8u); \
    }

static void drive_led()
{
    static uint64_t last = 0;
    uint64_t now = time_us_64();
    if (now - last < 4000) { // no faster than 250Hz
        return;
    }
    last = now;

    DRIVE_LED_PIO(pio0, 0, buf_base);
    DRIVE_LED_PIO(pio0, 1, buf_left);
    DRIVE_LED_PIO(pio0, 2, buf_right);
}

static inline uint32_t apply_level(uint32_t color)
{
    unsigned r = (color >> 16) & 0xff;
    unsigned g = (color >> 8) & 0xff;
    unsigned b = color & 0xff;

    r = r * groove_cfg->light.level / 255;
    g = g * groove_cfg->light.level / 255;
    b = b * groove_cfg->light.level / 255;

    return r << 16 | g << 8 | b;
}

void light_init()
{
    int offset = pio_add_program(pio0, &ws2812_program);
    gpio_set_drive_strength(BASE_RGB_PIN, GPIO_DRIVE_STRENGTH_2MA);
    ws2812_program_init(pio0, 0, offset, BASE_RGB_PIN, 800000, false);
    ws2812_program_init(pio0, 1, offset, LEFT_RGB_PIN, 800000, false);
    ws2812_program_init(pio0, 2, offset, RIGHT_RGB_PIN, 800000, false);
}

void light_update()
{
    drive_led();
}

uint32_t * const gimbal_leds[2][2] = {
    { buf_left, buf_base },
    { buf_right, buf_base + 11 },
};

uint32_t * const indicator_leds = buf_base + 8;

void light_set_left(int layer, int id, uint32_t color)
{
    if ((layer > 1) || (id > 7)) {
        return;
    }
    gimbal_leds[0][layer][id] = apply_level(color);
}

void light_set_right(int layer, int id, uint32_t color)
{
    if ((layer > 1) || (id > 7)) {
        return;
    }
    gimbal_leds[0][layer][id] = apply_level(color);
}

void light_set_center(int id, uint32_t color)
{
    if (id > 3) {
        return;
    }
    indicator_leds[id] = apply_level(color);
}
