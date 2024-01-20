/* Created by lda on 08-01-2024

   This source file describe NES architecture, including
   components like CPU, PPU, APU, etc.
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#define CACHE_SIZE      6

typedef enum RUN_MODE {
    NORMAL,
    STEP,
    PAUSE,
} run_mode_t;

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
    uint8_t mem[CPU_MEM_SIZE];

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
    bool prev_nmi;
    bool prev_irq;
    bool nmi_pending;
    bool irq_pending;
    bool prev_nmi_pending;
    bool prev_irq_pending;
    bool raise_nmi;
    bool raise_irq;

    struct memory_access_record record[10];
    int record_index;

    // debugging purpose
    uint16_t current_pc;
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

struct ppu {
    /* registers */
    union {
        uint8_t ppuctrl;
        struct {
            uint8_t NN : 2;     /* name table select */
            uint8_t I : 1;      /* increment mode */
            uint8_t S : 1;      /* sprite tile select */
            uint8_t BG : 1;      /* background tile select */
            uint8_t H : 1;      /* sprite height */
            uint8_t P : 1;      /* PPU master/slave */
            uint8_t NE : 1;      /* NMI enable */
        };
    };

    union {
        uint8_t ppumask;
        struct {
            uint8_t GR : 1;     /* grey scale */
            uint8_t m : 1;      /* background left column enable */
            uint8_t M : 1;      /* sprite left column enable */
            uint8_t b : 1;      /* background enable */
            uint8_t s : 1;      /* sprite enable */
            uint8_t R : 1;      /* red */
            uint8_t G : 1;      /* green */
            uint8_t B : 1;      /* blue */
        };
    };

    union {
        uint8_t ppustatus;
        struct {
            uint8_t UNUSED : 5;
            uint8_t O : 1;      /* sprite overflow */
            uint8_t SPR : 1;      /* sprite 0 hit */
            uint8_t VBL : 1;      /* vblank */
        };
    };

    uint8_t oamaddr;
    uint8_t ppuaddr;

    /* internal registers */
    uint16_t v : 15;    /* current VRAM address */
    uint16_t t : 15;    /* temporary VRAM address */
    uint16_t x : 3;     /* fine X scroll */
    uint16_t w : 1;     /* first or second write toggle */

    /* internal data bus */
    uint8_t io_db;

    /* internal memories(including OAM), not exposed with CPU */
    uint8_t mem[0x4000];
    uint8_t oam[256];
    uint8_t vram[2 * KB];

    /* NMI registers */
    bool nmi_occured;
    bool nmi_output;

    /* timing - related */
    int cycles;
    int scanlines;

    /* others */
    uint8_t scroll_offset[2];
    uint8_t read_buffer;
};

struct nes {
    run_mode_t run_mode;
    bool step;
    struct cpu cpu;
    struct cart cart;
    struct ppu ppu;

    /* for disassembler */
    uint16_t instr_addr_cache[CACHE_SIZE];
    int cache_index;
    int cache_size;
};

#ifdef __cplusplus
}
#endif