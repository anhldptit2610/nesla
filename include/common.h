#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MEM_SIZE    0x10000
#define STACK_BASE  0x0100

/* macros */
#define LSB(n)                  (((uint16_t)n >> 0) & 0x00ff)
#define MSB(n)                  (((uint16_t)n >> 8) & 0x00ff)
#define TO_U16(lsb, msb)        (((uint16_t)msb << 8) | ((uint16_t)lsb))

#define BIT(f, i)               (((f) & (1U << i)) >> i)
/* WARNING: only use this macro inside instructions */
#define P_BIT(i)               ((nes->cpu.p & (1U << i)) >> i)

#define SET_FLAG(f, i)          f |= (1U << i)
#define RESET_FLAG(f, i)        f &= ~(1U << i)
#define ASSIGN_FLAG(f, i, v)    f = (v) ? f | (1U << i) : f & ~(1U << i)
