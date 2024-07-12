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

static uint32_t buf_base[19]; // 8 + 3 + 8
static uint32_t buf_left[12];
static uint32_t buf_right[12];

#define PIPE_DEPTH 24
static uint32_t boost_base[2][2];
static uint32_t pipe_boost[2][PIPE_DEPTH];
static uint32_t pipe_steer[2][8][PIPE_DEPTH];

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

static uint32_t rgb32_fade(uint32_t color, uint8_t level)
{
    uint8_t r = (color >> 16) & 0xff;
    uint8_t g = (color >> 8) & 0xff;
    uint8_t b = color & 0xff;

    r = r * level / 255;
    g = g * level / 255;
    b = b * level / 255;

    return r << 16 | g << 8 | b;
}

static uint32_t rgb32_add(uint32_t c1, uint32_t c2)
{
    uint8_t r1 = (c1 >> 16) & 0xff;
    uint8_t g1 = (c1 >> 8) & 0xff;
    uint8_t b1 = c1 & 0xff;

    uint8_t r2 = (c2 >> 16) & 0xff;
    uint8_t g2 = (c2 >> 8) & 0xff;
    uint8_t b2 = c2 & 0xff;

    uint8_t r = r1 | r2;
    uint8_t g = g1 | g2;
    uint8_t b = b1 | b2;

    return r << 16 | g << 8 | b;
}

#define DRIVE_LED_PIO(pio, sm, buf) \
    for (int i = 0; i < count_of(buf); i++) { \
        pio_sm_put_blocking(pio, sm, buf[i] << 8u); \
    }

static void drive_led()
{
    static uint64_t last = 0;
    uint64_t now = time_us_64();
    if (now - last < 5000) { // 250Hz
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
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, BASE_RGB_PIN, 800000, false);
    ws2812_program_init(pio0, 1, offset, LEFT_RGB_PIN, 800000, false);
    ws2812_program_init(pio0, 2, offset, RIGHT_RGB_PIN, 800000, false);
}

static uint32_t * const gimbal_leds[2][2] = {
    { buf_left, buf_base },
    { buf_right, buf_base + 11 },
};

static void mix_left(int layer, int id, uint32_t color)
{
    if ((layer > 1) || (id > 7)) {
        return;
    }
    gimbal_leds[0][layer][id] = rgb32_add(boost_base[0][layer], apply_level(color));  
}

static void mix_right(int layer, int id, uint32_t color)
{
    if ((layer > 1) || (id > 7)) {
        return;
    }
    gimbal_leds[1][layer][id] = rgb32_add(boost_base[1][layer], apply_level(color));
}

static void effect_button()
{
    int phase = (time_us_32() / 50000) % 4;
    for (int i = 0; i < 4; i++) {
        uint32_t color = (phase == i) ? 0x808080 : 0x00;
        light_set_button(i, color);
        light_set_button(7 - i, color);
    }
}

static void run_pipe(uint32_t pipe[PIPE_DEPTH])
{
    for (int i = PIPE_DEPTH - 1; i > 0; i--) {
        pipe[i] = pipe[i - 1];
    }
    pipe[0] = rgb32_fade(pipe[0], 236);
}

static void effect_boost()
{
    for (int id = 0; id < 2; id++) {
        run_pipe(pipe_boost[id]);
    }
}

static void effect_steer()
{
    for (int id = 0; id < 2; id++) {
        for (int dir = 0; dir < 8; dir++) {
            run_pipe(pipe_steer[id][dir]);
        }
    }
}

static void effect_mix()
{
    for (int i = 0; i < 8; i++) {
        uint32_t left_0 = rgb32_add(pipe_boost[0][0], pipe_steer[0][i][0]);
        uint32_t left_1 = rgb32_add(pipe_boost[0][PIPE_DEPTH - 1], pipe_steer[0][i][PIPE_DEPTH - 1]);
        uint32_t right_0 = rgb32_add(pipe_boost[1][0], pipe_steer[1][i][0]);
        uint32_t right_1 = rgb32_add(pipe_boost[1][PIPE_DEPTH - 1], pipe_steer[1][i][PIPE_DEPTH - 1]);
        mix_left(0, i, left_0);
        mix_left(1, i, left_1);
        mix_right(0, i, right_0);
        mix_right(1, i, right_1);
    }
}

void light_effect()
{
    effect_button();

    static uint64_t last = 0;
    uint64_t now = time_us_64();
    if (now - last > 5000) { // 200Hz
        effect_boost();
        effect_steer();
        last = now;
    }

    effect_mix();
}

void light_update()
{
    drive_led();
}

void light_set_button(int id, uint32_t color)
{
    if (id < 4) {
        buf_left[8 + id] = apply_level(color);
    } else if (id < 8) {
        buf_right[8 + id - 4] = apply_level(color);
    } else if (id < 11) {
        buf_base[8 + id - 8] = apply_level(color);
    }
}

void light_set_boost_base(int id, int layer, uint32_t color)
{
    if (id >= 2) {
        return;
    }

    boost_base[id][layer] = color;
}

void light_set_boost(int id, uint32_t color)
{
    if (id >= 2) {
        return;
    }

    pipe_boost[id][0] = color;
}

void light_set_steer(int id, int dir, uint32_t color)
{
    if (id >= 2) {
        return;
    }

    dir %= 8;

    if (id < 3) {
        buf_base[16 + id] = apply_level(color);
    }
}
