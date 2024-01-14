#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "nes.h"
#include "mmu.h"

void cpu_step(struct nes *nes);
void cpu_at_power_up(struct nes *nes);
void cpu_get_opcode_info(char *ret, uint8_t opcode);

#ifdef __cplusplus
}
#endif