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

#include "gimbal.h"

#include "usb_descriptors.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static void disp_axis()
{
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           groove_cfg->hid.joy ? "on" : "off",
           groove_cfg->hid.nkro ? "on" : "off" );
}

static inline int sprintf_hsv_rgb(char *buf, const rgb_hsv_t *color)
{
    return sprintf(buf, "%s(%d,%d,%d)", color->rgb_hsv ? "hsv" : "rgb",
              color->val[0], color->val[1], color->val[2]);
}

static const char *color_str(const rgb_hsv_t *color, bool left_right)
{
    static char buf[64];
    int count = 0;

    if (left_right) {
        count += sprintf(buf + count, "LEFT ");
    }

    count += sprintf_hsv_rgb(buf + count, color);
    
    if (left_right) {
        count += sprintf(buf + count, ", RIGHT ");
        count += sprintf_hsv_rgb(buf + count, color + 1);
    }

    return buf;
}

static void disp_light()
{
    printf("[Light]\n");
    printf("  Level: %d.\n", groove_cfg->light.level);
    printf("  Colors:\n");
    printf("    base0: %s\n", color_str(groove_cfg->light.base[0], true));
    printf("    base1: %s\n", color_str(groove_cfg->light.base[0], true));
    printf("    button: %s\n", color_str(groove_cfg->light.button, true));
    printf("    boost: %s\n", color_str(groove_cfg->light.boost, true));
    printf("    steer: %s\n", color_str(groove_cfg->light.steer, true));
    printf("    aux_on: %s\n", color_str(&groove_cfg->light.aux_on, false));
    printf("    aux_off: %s\n", color_str(&groove_cfg->light.aux_off, false));
}

static void disp_haptics()
{
    printf("[Haptics]\n");
    printf("  Enabled: %s.\n", groove_cfg->haptics.enabled ? "on" : "off");
}

static void disp_gimbal()
{
    printf("[Gimbal]\n");
    const char *axis_names[] = {"LX", "LY", "RX", "RY"};
    for (int i = 0; i < 4; i++) {
        printf("  %s: %s, %s, deadzone %d, raw %d-%d-%d.\n",
               axis_names[i],
               groove_cfg->axis[i].invert ? "invert" : "normal",
               groove_cfg->axis[i].analog ? "analog" : "digital",
               groove_cfg->axis[i].deadzone,
               groove_cfg->axis[i].min, groove_cfg->axis[i].center, groove_cfg->axis[i].max);
    }
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [axis|light|haptics|hid|gimbal]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_axis();
        disp_light();
        disp_gimbal();
        disp_haptics();
        disp_hid();
        return;
    }

    const char *choices[] = {"axis", "light", "gimbal", "haptics", "hid"};
    switch (cli_match_prefix(choices, count_of(choices), argv[0])) {
        case 0:
            disp_axis();
            break;
        case 1:
            disp_light();
            break;
        case 2:
            disp_gimbal();
            break;
        case 3:
            disp_haptics();
            break;
        case 4:
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

    groove_cfg->light.level = level;
    config_changed();
    disp_light();
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

static void calibrate_center(size_t retry)
{
    uint32_t centers[4] = { 0 };
    for (int i = 0; i < retry; i++) {
        for (int axis = 0; axis < 4; axis++) {
            centers[axis] += gimbal_raw(axis);
        }
        sleep_ms(7);
    }
    for (int axis = 0; axis < 4; axis++) {
        groove_cfg->axis[axis].center = centers[axis] / retry;
    }
}

static void calibrate_range(uint32_t seconds)
{
    uint32_t mins[4], maxs[4];
    for (int axis = 0; axis < 4; axis++) {
        mins[axis] = groove_cfg->axis[axis].center;
        maxs[axis] = groove_cfg->axis[axis].center;
    }

    uint64_t start = time_us_64();

    while (time_us_64() - start < seconds * 1000000) {
        for (int axis = 0; axis < 4; axis++) {
            uint16_t val = gimbal_raw(axis);
            if (val < mins[axis]) {
                mins[axis] -= (mins[axis] - val) / 2;
            } else if (val > maxs[axis]) {
                maxs[axis] += (val - maxs[axis]) / 2;
            }
        }
        sleep_ms(7);
    }

    for (int axis = 0; axis < 4; axis++) {
        mins[axis] += (groove_cfg->axis[axis].center - mins[axis]) / 16;
        maxs[axis] -= (maxs[axis] - groove_cfg->axis[axis].center) / 16;
        groove_cfg->axis[axis].min = mins[axis];
        groove_cfg->axis[axis].max = maxs[axis];
    }
}

static void gimbal_calibrate()
{
    printf("Caliration Steps:\n"
           "  1. Make sure the gimbals are at the center position.\n"
           "  2. Enter this calibrate command.\n"
           "  3. Rotate both gimbals to the full range of motion several times.\n"
           "  4. Calibration finishes in 5 seconds.\n");

    printf("Now calibrating centers ...");
    fflush(stdout);

    calibrate_center(10);
    printf(" done.\n");

    printf("Now calibrating range ...");
    fflush(stdout);

    calibrate_range(5);
    printf(" done.\n");
}

static void gimbal_invert(int axis, const char *param)
{
    const char *usage = "Usage: gimbal <all|lx|ly|rx|ry> invert <on|off>\n";

    int invert = cli_match_prefix((const char *[]){"off", "on"}, 2, param);
    if (invert < 0) {
        printf(usage);
        return;
    }

    printf("ax:%d, param:%s, invert:%d\n", axis, param, invert);

    for (int i = 0; i < 4; i++) {
        if ((i == axis) || (axis == 4)) {
            groove_cfg->axis[i].invert = invert;
        }
    }
}

static void gimbal_deadzone(int axis, const char *param)
{
    const char *usage = "Usage: gimbal <all|lx|ly|rx|ry> deadzone <0..100>\n";
    int deadzone = cli_extract_non_neg_int(param, 0);
    if ((deadzone < 0) || (deadzone > 100)) {
        printf(usage);
        return;
    }

    for (int i = 0; i < 4; i++) {
        if ((i == axis) || (axis == 4)) {
            groove_cfg->axis[i].deadzone = deadzone;
        }
    }
}

static void gimbal_analog(int axis, const char *param)
{
    const char *usage = "Usage: gimbal <all|lx|ly|rx|ry> analog <on|off>\n";
    int analog = cli_match_prefix((const char *[]){"off", "on"}, 2, param);
    if (analog < 0) {
        printf(usage);
        return;
    }

    for (int i = 0; i < 4; i++) {
        if ((i == axis) || (axis == 4)) {
            groove_cfg->axis[i].analog = analog;
        }
    }
}

static void handle_gimbal(int argc, char *argv[])
{
    const char *usage = "Usage: gimbal calibrate\n"
                        "       gimbal invert <all|lx|ly|rx|ry> <on|off>\n"
                        "       gimbal deadzone <all|lx|ly|rx|ry> <0..100>\n"
                        "       gimbal analog <all|lx|ly|rx|ry> <on|off>\n";
    if (argc == 1) {
        if (strncasecmp(argv[0], "calibrate", strlen(argv[0])) != 0) {
            printf(usage);
            return;
        }
        gimbal_calibrate();
    } else if (argc == 3) {
        int axis = cli_match_prefix((const char *[]){"lx", "ly", "rx", "ry", "all"}, 5, argv[1]);
        if (axis < 0) {
            printf(usage);
            return;
        }

        int op = cli_match_prefix((const char *[]){"invert", "deadzone", "analog"}, 3, argv[0]);
        if (op == 0) {
            gimbal_invert(axis, argv[2]);
        } else if (op == 1) {
            gimbal_deadzone(axis, argv[2]);
        } else if (op == 2) {
            gimbal_analog(axis, argv[2]);
        } else {
            printf(usage);
            return;
        }
    } else {
        printf(usage);
        return;
    }

    config_changed();
    disp_gimbal();
}

static bool extract_color(rgb_hsv_t *color, char *argv[4])
{
    int rgb_hsv = cli_match_prefix((const char *[]){"rgb", "hsv"}, 2, argv[0]);
    if (rgb_hsv < 0) {
        return false;
    }
    color->rgb_hsv = rgb_hsv;

    for (int i = 0; i < 3; i++) {
        int v = cli_extract_non_neg_int(argv[1 + i], 0);
        if ((v < 0) || (v > 255)) {
            return false;
        }
        color->val[i] = v;
    }

    return true;
}

static void handle_color(int argc, char *argv[])
{
    const char *usage = "Usage: color <name> [left|right] <rgb|hsv> <0..255> <0..255> <0..255>\n"
                        "  name: base0 base1 button boost steer aux_on aux_off\n";
    if ((argc != 5) && (argc != 6)) {
        printf(usage);
        return;
    }

    rgb_hsv_t *names[] = {
        &groove_cfg->light.aux_on,
        &groove_cfg->light.aux_off,
        groove_cfg->light.base[0],
        groove_cfg->light.base[1],
        groove_cfg->light.button,
        groove_cfg->light.boost,
        groove_cfg->light.steer,
    };
    const char *choices[] = {"aux_on", "aux_off", "base0", "base1", "button", "boost", "steer"};
    static_assert(count_of(choices) == count_of(names));

    int name = cli_match_prefix(choices, count_of(choices), argv[0]);
    if (name < 0) {
        printf(usage);
        return;
    }

    bool left = true;
    bool right = true;
    if (argc == 6) {
        int left_right = cli_match_prefix((const char *[]){"left", "right"}, 2, argv[1]);
        if (left_right < 0) {
            printf(usage);
            return;
        }
        left = (left_right == 0);
        right = (left_right == 1);
    }

    rgb_hsv_t color;
    if (!extract_color(&color, argv + argc - 4)) {
        printf(usage);
        return;
    }

    rgb_hsv_t *target = names[name];
    if (left) {
        target[0] = color;
    }
    if ((name >= 2) && right) {
        target[1] = color;
    }

    config_changed();
    disp_light();
}

static void handle_haptics(int argc, char *argv[])
{
    const char *usage = "Usage: haptics <on|off>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int on_off = cli_match_prefix((const char *[]){"off", "on"}, 2, argv[0]);
    if (on_off < 0) {
        printf(usage);
        return;
    }

    groove_cfg->haptics.enabled = on_off;
    config_changed();
    disp_haptics();
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
    cli_register("color", handle_color, "Set LED color.");
    cli_register("hid", handle_hid, "Set HID mode.");
    cli_register("gimbal", handle_gimbal, "Calibrate the gimbals.");
    cli_register("haptics", handle_haptics, "Enable/disable haptics.");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
}
