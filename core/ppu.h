#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "nes.h"

void ppu_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode);
void ppu_tick(struct nes *nes);
void ppu_at_power_up(struct nes *nes);

#ifdef __cplusplus
}
#endif