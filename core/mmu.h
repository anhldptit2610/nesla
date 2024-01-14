#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "nes.h"
#include "cart.h"
#include "ppu.h"
#include "apu.h"

uint8_t mmu_read(struct nes *nes, uint16_t addr);
void mmu_write(struct nes *nes, uint16_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif