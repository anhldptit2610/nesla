/* Created by lda on 08-01-2024

   This source file describe NES architecture, including
   components like CPU, PPU, APU, etc.
*/

#pragma once

#include "common.h"

struct cpu {
    /* registers */
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint16_t pc;
    uint8_t sp;
    union {
        uint8_t p;
        struct {
            uint8_t C_FLAG : 1;
            uint8_t Z_FLAG : 1;
            uint8_t I_FLAG : 1;
            uint8_t D_FLAG : 1;
            uint8_t B_FLAG : 1;
            uint8_t UNUSED : 1;
            uint8_t V_FLAG : 1;
            uint8_t N_FLAG : 1;
        };
    };

    /* instructions decoder */
    uint8_t opcode;
    uint8_t operand[2];
    uint8_t operation_value;
    uint16_t effective_addr;
    bool page_boundary_crossed;
};

struct nes {
    uint8_t mem[MEM_SIZE];
    struct cpu cpu;
};
