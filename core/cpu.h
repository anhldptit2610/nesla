#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "nes.h"
#include "mmu.h"
#include "interrupt.h"
#include "ppu.h"

typedef enum ADDRESSING_MODE {
    IMPL,    /* implicit */
    ACC,    /* accumulator */
    IMM,    /* immediate */
    ZP,     /* zero page */
    ABS,    /* absolute */
    REL,    /* relative */
    IND,    /* indirect */
    ZPX,    /* zero page indexed x */
    ZPY,    /* zero page indexed y */
    ABSX,   /* absolute indexed x */
    ABSY,   /* absolute indexed y */
    XIND,   /* indexed x indirect  */
    INDY,   /* indirect indexed y */
    NONE,
} addr_mode_t;

void cpu_step(struct nes *nes);
void cpu_at_power_up(struct nes *nes);
void cpu_get_opcode_info(char *ret, uint8_t opcode);
uint8_t cpu_read(struct nes *nes, uint16_t addr);
void cpu_write(struct nes *nes, uint16_t addr, uint8_t val);
void stack_push_8(struct nes *nes, uint8_t data);
void stack_push_16(struct nes *nes, uint16_t data);

void get_opcode_name(uint8_t opcode, char *ret);
addr_mode_t get_opcode_mode(uint8_t opcode);
void handle_addressing_mode(struct nes *nes, addr_mode_t addr_mode);

void cache_push(struct nes *nes, uint16_t addr);
uint16_t cache_get_nth_element(struct nes *nes, int n);

#ifdef __cplusplus
}
#endif