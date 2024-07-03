/*
 * Controller Config
 * WHowe <github.com/whowechina>
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    struct {
        uint32_t key_on;
        uint32_t key_off;
        uint32_t gap;
    } colors;
    struct {
        uint8_t key;
        uint8_t gap;
        uint8_t tof;
        uint8_t level;
    } style;
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
