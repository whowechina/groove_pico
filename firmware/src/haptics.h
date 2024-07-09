/*
 * Haptic Feedback
 * WHowe <github.com/whowechina>
 */

#ifndef HAPTICS_H
#define HAPTICS_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/flash.h"

void haptics_init();
void haptics_set(int id, bool on);

#endif
