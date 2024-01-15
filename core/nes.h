/* Created by lda on 08-01-2024

   This source file describe NES architecture, including
   components like CPU, PPU, APU, etc.
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

typedef enum MEM_MODE {
    READ,
    WRITE
} mem_mode_t;

typedef enum INTERRUPT {
    NMI = (1U << 0),
    IRQ = (1U << 1),
    RESET = (1U << 2),
    BRK = (1U << 3)
} interrupt_t;

struct memory_access_record {
    uint16_t addr;
    uint8_t val;
    enum r_w {
        R,
        W,
    } action;
};

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
            uint8_t C : 1;
            uint8_t Z : 1;
            uint8_t I : 1;
            uint8_t D : 1;
            uint8_t B : 1;
            uint8_t UNUSED : 1;
            uint8_t V : 1;
            uint8_t N : 1;
        };
    };

    /* instructions decoder */
    uint8_t opcode;
    uint8_t operand[2];
    uint8_t operation_value;
    uint16_t effective_addr;
    uint16_t non_effective_addr;
    bool page_boundary_crossed;

    /* interrupt */
    bool nmi;
    bool irq;
    uint8_t interrupt_pending;

    struct memory_access_record record[10];
    int record_index;
};

struct rom_info {
    uint32_t prg_size;
    uint32_t chr_size;
    uint32_t prg_ram_size;
    uint8_t mapper;
    enum MIRRORING {
        HORIZONTAL,
        VERTICAL
    } mirroring;
    uint8_t type : 2;
};

struct cart {
    uint8_t *prg_rom;
    uint8_t *chr_rom;
    struct rom_info info;
};

struct nes {
    uint8_t mem[MEM_SIZE];
    struct cpu cpu;
    struct cart cart;
};

#ifdef __cplusplus
}
#endif