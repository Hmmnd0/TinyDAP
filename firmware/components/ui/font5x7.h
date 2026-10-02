#pragma once

#include <stdint.h>

/* Classic 5x7 ASCII font, 0x20..0x7E. One byte per column, LSB = top row. */
extern const uint8_t font5x7[95][5];
