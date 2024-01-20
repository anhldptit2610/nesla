#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define KB          1024

#define CPU_MEM_SIZE            0x10000
#define PPU_MEM_SIZE            0x4000
#define STACK_BASE          0x0100
#define SCREEN_HEIGHT       240
#define SCREEN_WIDTH        256


/* macros */
#define LSB(n)                  (((uint16_t)n >> 0) & 0x00ff)
#define MSB(n)                  (((uint16_t)n >> 8) & 0x00ff)
#define TO_U16(lsb, msb)        (((uint16_t)msb << 8) | ((uint16_t)lsb))

#define BIT(f, i)               (((f) & (1U << i)) >> i)

#ifdef __cplusplus
}
#endif