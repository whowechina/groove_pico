/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "board_defs.h"

#include "save.h"
#include "config.h"
#include "cli.h"
#include "commands.h"

#include "light.h"
#include "button.h"
#include "gimbal.h"
#include "haptics.h"

struct __attribute__((packed)) {
    uint16_t buttons;
    uint8_t HAT;
	uint8_t lx;
	uint8_t ly;
	uint8_t rx;
	uint8_t ry;
    uint8_t vendor;
} hid_joy, sent_hid_joy;

struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keymap[15];
} hid_nkro, sent_hid_nkro;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        if (groove_cfg->hid.joy && 
            (memcmp(&hid_joy, &sent_hid_joy, sizeof(hid_joy)) != 0)) {
            if (tud_hid_n_report(0x00, 0, &hid_joy, sizeof(hid_joy))) {
                memcpy(&sent_hid_joy, &hid_joy, sizeof(hid_joy));
            }
        }
        if (groove_cfg->hid.nkro) {
            sent_hid_nkro = hid_nkro;
            tud_hid_n_report(0x02, 0, &sent_hid_nkro, sizeof(sent_hid_nkro));
        }
    }
}

#define SWITCH_BIT_Y       (1U <<  0)
#define SWITCH_BIT_B       (1U <<  1)
#define SWITCH_BIT_A       (1U <<  2)
#define SWITCH_BIT_X       (1U <<  3)
#define SWITCH_BIT_L       (1U <<  4)
#define SWITCH_BIT_R       (1U <<  5)
#define SWITCH_BIT_ZL      (1U <<  6)
#define SWITCH_BIT_ZR      (1U <<  7)
#define SWITCH_BIT_MINUS   (1U <<  8)
#define SWITCH_BIT_PLUS    (1U <<  9)
#define SWITCH_BIT_L3      (1U << 10)
#define SWITCH_BIT_R3      (1U << 11)
#define SWITCH_BIT_HOME    (1U << 12)

#define BUTTON_SHIFT 0x08
#define BUTTON_AUX1 0x04
#define BUTTON_AUX2 0x10
#define BUTTON_LEFT 0x01
#define BUTTON_RIGHT 0x02

static void gen_joy_report()
{
    hid_joy.vendor = 0;
    hid_joy.buttons = 0;
    hid_joy.HAT = 0x08;

    uint16_t button = button_read();
    if (button & BUTTON_SHIFT) {
        hid_joy.lx = 128;
        hid_joy.ly = 128;
        hid_joy.rx = 128;
        hid_joy.ry = 128;
        hid_joy.buttons |= (button & BUTTON_LEFT) ? SWITCH_BIT_Y : 0;
        hid_joy.buttons |= (button & BUTTON_RIGHT) ? SWITCH_BIT_X : 0;
        hid_joy.buttons |= (button & BUTTON_AUX1) ? SWITCH_BIT_MINUS : 0;
        hid_joy.buttons |= (button & BUTTON_AUX2) ? SWITCH_BIT_PLUS : 0;

        static bool dpad_active = false;
        static uint8_t last_dpad = 0x08;

        if (dpad_active) {
            if (gimbal_get_amp(0) < 1200) {
                dpad_active = false;
                last_dpad = 0x08;
            } else {
                last_dpad = gimbal_get_dir(0);
            }
        } else {
            if (gimbal_get_amp(0) > 1600) {
                dpad_active = true;
                last_dpad = gimbal_get_dir(0);
            }
        }

        hid_joy.HAT = last_dpad;
    } else {
        hid_joy.lx = gimbal_read(GIMBAL_LEFT_X) >> 4;
        hid_joy.ly = gimbal_read(GIMBAL_LEFT_Y) >> 4;
        hid_joy.rx = gimbal_read(GIMBAL_RIGHT_X) >> 4;
        hid_joy.ry = gimbal_read(GIMBAL_RIGHT_Y) >> 4;
        hid_joy.buttons |= (button & BUTTON_LEFT) ? SWITCH_BIT_L : 0;
        hid_joy.buttons |= (button & BUTTON_RIGHT) ? SWITCH_BIT_R : 0;
        hid_joy.buttons |= (button & BUTTON_AUX1) ? SWITCH_BIT_B : 0;
        hid_joy.buttons |= (button & BUTTON_AUX2) ? SWITCH_BIT_A : 0;
    }
}

const uint8_t keycode_table[128][2] = { HID_ASCII_TO_KEYCODE };
const uint8_t keymap[] = NKRO_KEYMAP; // 32 keys, 6 air keys, 1 terminator

static void nkro_markup(uint8_t ascii, bool pressed)
{
    if (ascii >= 128) {
        return;
    }
    uint8_t code = keycode_table[ascii][1];
    uint8_t byte = code / 8;
    uint8_t bit = code % 8;
    if (pressed) {
        hid_nkro.keymap[byte] |= (1 << bit);
    } else {
        hid_nkro.keymap[byte] &= ~(1 << bit);
    }
}

static void gen_nkro_dir(int channel)
{
    int amp = gimbal_get_amp(channel);
    if (amp < 1600) {
        return;
    }
    const uint8_t *dirkey = &keymap[5] + channel * 4;
    nkro_markup(dirkey[0], false);
    nkro_markup(dirkey[0], false);
    nkro_markup(dirkey[0], false);
    nkro_markup(dirkey[0], false);

    int dir = gimbal_get_dir(channel);
    nkro_markup(dirkey[0], (dir == 0) || (dir == 1) || (dir == 7));
    nkro_markup(dirkey[1], (dir == 1) || (dir == 2) || (dir == 3));
    nkro_markup(dirkey[2], (dir == 3) || (dir == 4) || (dir == 5));
    nkro_markup(dirkey[3], (dir == 5) || (dir == 6) || (dir == 7));
}

static void gen_nkro_report()
{
    if (!groove_cfg->hid.nkro) {
        return;
    }

    uint16_t button = button_read();
    for (int i = 0; i < 5; i++) {
        nkro_markup(keymap[i], button & (1 << i));
    }

    gen_nkro_dir(0);
    gen_nkro_dir(1);
}

static uint64_t last_hid_time = 0;

static void run_lights()
{
    uint16_t button = button_read();
    if (button & 0x01) {
        light_boost_left();
    }
    if (button & 0x02) {
        light_boost_right();
    }

    light_set_aux(0, button & 0x04);
    light_set_aux(1, button & 0x08);
    light_set_aux(2, button & 0x10);

    if (gimbal_get_amp(0) > 1600) {
        light_steer_left(gimbal_get_dir(0));
    }
    if (gimbal_get_amp(1) > 1600) {
        light_steer_right(gimbal_get_dir(1));
    }
}

static void run_haptics()
{
    uint16_t button = button_read();
    haptics_set(0, button & 0x01);
    haptics_set(1, button & 0x02);
}

static mutex_t core1_io_lock;
static void core1_loop()
{
    while (1) {
        if (mutex_try_enter(&core1_io_lock, NULL)) {
            run_lights();
            run_haptics();
            light_update();
            mutex_exit(&core1_io_lock);
        }
        cli_fps_count(1);
        sleep_us(700);
    }
}

static void core0_loop()
{
    uint64_t next_frame = 0;
    while(1) {
        tud_task();

        cli_run();
    
        save_loop();
        cli_fps_count(0);

        button_update();

        gen_joy_report();
        gen_nkro_report();
        report_usb_hid();
    
        sleep_until(next_frame);
        next_frame += 1001; // Slightly lower than 1KHz
    }
}

/* if certain key pressed when booting, enter update mode */
static void update_check()
{
    const uint8_t pins[] = BUTTON_DEF; // keypad 00 and *
    bool all_pressed = true;
    for (int i = 0; i < 2; i++) {
        uint8_t gpio = pins[sizeof(pins) - 2 + i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        sleep_ms(1);
        if (gpio_get(gpio)) {
            all_pressed = false;
            break;
        }
    }

    if (all_pressed) {
        sleep_ms(100);
        reset_usb_boot(0, 2);
        return;
    }
}

void init()
{
    sleep_ms(50);
    board_init();

    update_check();

    tusb_init();
    stdio_init_all();

    config_init();
    mutex_init(&core1_io_lock);
    save_init(0xca44caac, &core1_io_lock);

    light_init();
    button_init();
    gimbal_init();
    haptics_init();

    cli_init("groove_pico>", "\n   << Groove Pico Controller >>\n"
                            " https://github.com/whowechina\n\n");
    
    commands_init();
}

int main(void)
{
    init();
    multicore_launch_core1(core1_loop);
    core0_loop();
    return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        last_hid_time = time_us_64();
        return;
    } 
}
