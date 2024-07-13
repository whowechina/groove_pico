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
} hid_joy;

struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keymap[15];
} hid_nkro, sent_hid_nkro;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        hid_joy.HAT = 0x08;
        hid_joy.vendor = 0;
        if (groove_cfg->hid.joy) {
            tud_hid_n_report(0x00, 0, &hid_joy, sizeof(hid_joy));
        }
        if (groove_cfg->hid.nkro &&
            (memcmp(&hid_nkro, &sent_hid_nkro, sizeof(hid_nkro)) != 0)) {
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

static void gen_joy_report()
{
    hid_joy.lx = gimbal_read(GIMBAL_LEFT_X) >> 4;
    hid_joy.ly = gimbal_read(GIMBAL_LEFT_Y) >> 4;
    hid_joy.rx = gimbal_read(GIMBAL_RIGHT_X) >> 4;
    hid_joy.ry = gimbal_read(GIMBAL_RIGHT_Y) >> 4;

    uint16_t button = button_read();
    hid_joy.buttons = 0;
    hid_joy.buttons |= (button & 0x01) ? SWITCH_BIT_L : 0;
    hid_joy.buttons |= (button & 0x02) ? SWITCH_BIT_R : 0;
    if (button & 0x08) {
        hid_joy.buttons |= (button & 0x04) ? SWITCH_BIT_MINUS : 0;
        hid_joy.buttons |= (button & 0x10) ? SWITCH_BIT_PLUS : 0;
    } else {
        hid_joy.buttons |= (button & 0x04) ? SWITCH_BIT_B : 0;
        hid_joy.buttons |= (button & 0x10) ? SWITCH_BIT_A : 0;
    }
}

const uint8_t keycode_table[128][2] = { HID_ASCII_TO_KEYCODE };
const uint8_t keymap[38 + 1] = NKRO_KEYMAP; // 32 keys, 6 air keys, 1 terminator
static void gen_nkro_report()
{
    for (int i = 0; i < 6; i++) {
        uint8_t code = keycode_table[keymap[32 + i]][1];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (hid_joy.buttons & (1 << i)) {
            hid_nkro.keymap[byte] |= (1 << bit);
        } else {
            hid_nkro.keymap[byte] &= ~(1 << bit);
        }
    }
}

static uint64_t last_hid_time = 0;

static void run_lights()
{
    uint64_t now = time_us_64();
    if (now - last_hid_time >= 1000000) {
    }

    light_effect();

    int dir0 = gimbal_get_dir(0);
    int dir1 = gimbal_get_dir(1);

    if (dir0 >= 0) {
        light_set_steer(0, dir0, rgb32_from_hsv(80, 255, 255));
    }

    if (dir1 >= 0) {
        light_set_steer(1, dir1, rgb32_from_hsv(80, 255, 255));
    }

    for (int i = 0; i < 8; i++) {
        light_set_boost_base(0, 0, rgb32_from_hsv(20, 0, 1));
        light_set_boost_base(0, 1, rgb32_from_hsv(20, 0, 20));
        light_set_boost_base(1, 0, rgb32_from_hsv(20, 0, 1));
        light_set_boost_base(1, 1, rgb32_from_hsv(20, 0, 20));
    }

    uint16_t button = button_read();
    if (button & 0x01) {
        uint32_t color = rgb32_from_hsv(20, 255, 255);
        light_set_button(0, color);
        light_set_button(1, color);
        light_set_button(2, color);
        light_set_button(3, color);
        light_set_boost(0, color);
        haptics_set(0, true);
    } else {
        haptics_set(0, false);
    }

    if (button & 0x02) {
        uint32_t color = rgb32_from_hsv(147, 255, 255);
        light_set_button(4, color);
        light_set_button(5, color);
        light_set_button(6, color);
        light_set_button(7, color);
        light_set_boost(1, color);
        haptics_set(1, true);
    } else {
        haptics_set(1, false);
    }

    for (int i = 8; i < 11; i++) {
        light_set_button(i, rgb32_from_hsv(0, 0, 10));
    }

    if (button & 0x04) {
        light_set_button(8, rgb32_from_hsv(0, 0, 255));
    }
    if (button & 0x08) {
        light_set_button(9, rgb32_from_hsv(0, 0, 255));
    }
    if (button & 0x10) {
        light_set_button(10, rgb32_from_hsv(0, 0, 255));
    }
}

static mutex_t core1_io_lock;
static void core1_loop()
{
    while (1) {
        if (mutex_try_enter(&core1_io_lock, NULL)) {
            run_lights();
            light_update();
            mutex_exit(&core1_io_lock);
        }
        cli_fps_count(1);
        sleep_us(700);
    }
}

static void core0_loop()
{
    while(1) {
        tud_task();

        cli_run();
    
        save_loop();
        cli_fps_count(0);

        button_update();

        gen_joy_report();
        gen_nkro_report();
        report_usb_hid();
    
        sleep_us(600);
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
    save_init(0xca44cafe, &core1_io_lock);

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


struct __attribute__((packed)) {
    uint16_t buttons;
    uint8_t  HAT;
    uint32_t axis;
} hid_joy_out = {0};

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    printf("Get from USB %d-%d\n", report_id, report_type);
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
