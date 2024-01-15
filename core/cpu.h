#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "nes.h"
#include "mmu.h"
#include "interrupt.h"

void cpu_step(struct nes *nes);
void cpu_at_power_up(struct nes *nes);
void cpu_get_opcode_info(char *ret, uint8_t opcode);
uint8_t cpu_read(struct nes *nes, uint16_t addr);
void cpu_write(struct nes *nes, uint16_t addr, uint8_t val);
void stack_push_8(struct nes *nes, uint8_t data);
void stack_push_16(struct nes *nes, uint16_t data);

#ifdef __cplusplus
}
#endif