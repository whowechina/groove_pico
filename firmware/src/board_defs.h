/*
 * Groove Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_GROOVE_PICO

#define BASE_RGB_PIN 2 // 13
#define LEFT_RGB_PIN 12
#define RIGHT_RGB_PIN 11

#define RGB_ORDER GRB // or RGB

#define RGB_BUTTON_MAP { 2, 0, 1, 3, 4 }
#define BUTTON_DEF { 11, 9, 10, 12, 13, 1, 0}

#define AXIS_MUX_PIN_A 21
#define AXIS_MUX_PIN_B 20
#define ADC_CHANNEL 0

#define NKRO_KEYMAP "awsdjikl123"
#else

#endif
