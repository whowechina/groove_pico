/*
 * Controller Config
 * WHowe <github.com/whowechina>
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t rgb_hsv; // 0: RGB, 1: HSV
    uint8_t val[3]; // RGB or HSV
} rgb_hsv_t;

typedef struct __attribute__((packed)) {
    struct {
        uint16_t center;
        uint16_t min;
        uint16_t max;
        uint8_t deadzone:7;
        uint8_t invert:1;
        uint8_t threshold:7;
        uint8_t analog:1;
    } axis[4];
    struct {
        rgb_hsv_t base[2][2];
        rgb_hsv_t button[2];
        rgb_hsv_t boost[2];
        rgb_hsv_t steer[2];
        rgb_hsv_t aux_on;
        rgb_hsv_t aux_off;
        uint8_t level;
        uint8_t reserved[15];
    } light;
    struct {
        bool enabled;
        uint8_t reserved[3];
    } haptics;
    struct {
        uint8_t joy : 4;
        uint8_t nkro : 4;
    } hid;
} groove_cfg_t;

typedef struct {
    uint16_t fps[2];
} groove_runtime_t;

extern groove_cfg_t *groove_cfg;
extern groove_runtime_t *groove_runtime;

void config_init();
void config_changed(); // Notify the config has changed
void config_factory_reset(); // Reset the config to factory default

#endif
