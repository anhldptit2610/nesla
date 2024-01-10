#pragma once

#include "common.h"
#include "nes.h"

uint8_t mmu_read(struct nes *nes, uint16_t addr);
void mmu_write(struct nes *nes, uint16_t addr, uint8_t val);