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
    .colors = {
        .key_on = 0xff0000,
        .key_off = 0x000000,
    },
    .style = {
        .key = 0,
        .level = 127,
    },
    .hid = {
        .joy = 1,
        .nkro = 0,
    },
};

groove_runtime_t *groove_runtime;

static void config_loaded()
{
    if (groove_cfg->style.level > 10) {
        groove_cfg->style.level = default_cfg.style.level;
        config_changed();
    }
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
