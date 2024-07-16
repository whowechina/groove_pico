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
        { 1400, 2000, 2500, 20, 1, 80, 1 },
        { 1400, 2000, 2500, 20, 1, 80, 1 },
        { 1400, 2000, 2500, 20, 1, 80, 1 },
        { 1400, 2000, 2500, 20, 1, 80, 1 },
    },
    .light = {
        .level = 128,
        .base = {
            {
                { 1, { 20, 150, 10 }, },
                { 1, { 20, 150, 30 }, },
            },
            {
                { 1, { 147, 150, 10 }, },
                { 1, { 147, 150, 30 }, },
            },
        },
        .button = {
            { 1, { 0, 0, 120 } },
            { 1, { 0, 0, 120 } },
        },
        .boost = {
            { 1, { 20, 255, 255 } },
            { 1, { 147, 255, 255 } },
        },
        .steer = {
            { 1, { 80, 255, 255 } },
            { 1, { 80, 255, 255 } },
        },
        .aux_on = { 0, { 100, 100, 100 } },
        .aux_off = { 0, { 8, 8, 8 } },
        .reserved = { 0 },
    },
    .haptics = {
        .enabled = true,
        .reserved = { 0 },
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
