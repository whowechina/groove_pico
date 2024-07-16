/*
 * WS2812B Lights Control (Base + Left and Right Gimbals)
 * WHowe <github.com/whowechina>
 */

#ifndef LIGHT_H
#define LIGHT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"

void light_init();
void light_update();

uint32_t rgb32(uint32_t r, uint32_t g, uint32_t b, bool gamma_fix);
uint32_t rgb32_from_hsv(uint8_t h, uint8_t s, uint8_t v);
uint32_t load_color(const rgb_hsv_t *color);

void light_set_aux(int id, bool active);
void light_boost_left();
void light_boost_right();
void light_steer_left(int dir);
void light_steer_right(int dir);

#endif
