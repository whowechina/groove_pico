#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "save.h"
#include "cli.h"

#include "usb_descriptors.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static void disp_colors()
{
    printf("[Colors]\n");
    printf("  Key on: %06lx, off: %06lx\n", 
           groove_cfg->colors.key_on, groove_cfg->colors.key_off);
}

static void disp_style()
{
    printf("[Style]\n");
    printf("  Key: %d, Level: %d\n",
           groove_cfg->style.key, groove_cfg->style.level);
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           groove_cfg->hid.joy ? "on" : "off",
           groove_cfg->hid.nkro ? "on" : "off" );
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [colors|style|hid]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_colors();
        disp_style();
        disp_hid();
        return;
    }

    const char *choices[] = {"colors", "style", "hid"};
    switch (cli_match_prefix(choices, 3, argv[0])) {
        case 0:
            disp_colors();
            break;
        case 1:
            disp_style();
            break;
        case 2:
            disp_hid();
            break;
        default:
            printf(usage);
            break;
    }
}

static int fps[2];
void fps_count(int core)
{
    static uint32_t last[2] = {0};
    static int counter[2] = {0};

    counter[core]++;

    uint32_t now = time_us_32();
    if (now - last[core] < 1000000) {
        return;
    }
    last[core] = now;
    fps[core] = counter[core];
    counter[core] = 0;
}

static void handle_level(int argc, char *argv[])
{
    const char *usage = "Usage: level <0..255>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int level = cli_extract_non_neg_int(argv[0], 0);
    if ((level < 0) || (level > 255)) {
        printf(usage);
        return;
    }

    groove_cfg->style.level = level;
    config_changed();
    disp_style();
}

static void handle_hid(int argc, char *argv[])
{
    const char *usage = "Usage: hid <joy|nkro|both>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    const char *choices[] = {"joy", "nkro", "both"};
    int match = cli_match_prefix(choices, 3, argv[0]);
    if (match < 0) {
        printf(usage);
        return;
    }

    groove_cfg->hid.joy = ((match == 0) || (match == 2)) ? 1 : 0;
    groove_cfg->hid.nkro = ((match == 1) || (match == 2)) ? 1 : 0;
    config_changed();
    disp_hid();
}

static void handle_save()
{
    save_request(true);
}

static void handle_factory_reset()
{
    config_factory_reset();
    printf("Factory reset done.\n");
}

void commands_init()
{
    cli_register("display", handle_display, "Display all config.");
    cli_register("level", handle_level, "Set LED brightness level.");
    cli_register("hid", handle_hid, "Set HID mode.");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
}
