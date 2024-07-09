/*
 * Groove Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_GROOVE_PICO

#define BASE_RGB_PIN 13
#define LEFT_RGB_PIN 12
#define RIGHT_RGB_PIN 11

#define RGB_ORDER GRB // or RGB

#define BUTTON_DEF { 9, 8, 2, 1, 0 }
#define HAPTICS_DEF { 7, 6 }

#define AXIS_MUX_PIN_A 21
#define AXIS_MUX_PIN_B 20
#define ADC_CHANNEL 0

#define NKRO_KEYMAP "awsdjikl123"
#else

#endif
