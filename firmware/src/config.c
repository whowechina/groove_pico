/*
 * Controller Config and Runtime Data
 * WHowe <github.com/whowechina>
 * 
 * Config is a global data structure that stores all the configuration
 * Runtime is something to share between files.
 */

#include "config.h"
#include "save.h"

groove_cfg_t *groove_cfg;

static groove_cfg_t default_cfg = {
    .axis = {
        { 2048, 0, 4095, 127, 0, 70, 1 },
        { 2048, 0, 4095, 127, 0, 70, 1 },
        { 2048, 0, 4095, 127, 0, 70, 1 },
        { 2048, 0, 4095, 127, 0, 70, 1 },
    },
    .light = {
        .level = 128,
        .reserved = {0},
    },
    .hid = {
        .joy = 1,
        .nkro = 0,
    },
};

groove_runtime_t *groove_runtime;

static void config_loaded()
{
}

void config_changed()
{
    save_request(false);
}

void config_factory_reset()
{
    *groove_cfg = default_cfg;
    save_request(true);
}

void config_init()
{
    groove_cfg = (groove_cfg_t *)save_alloc(sizeof(*groove_cfg), &default_cfg, config_loaded);
}
