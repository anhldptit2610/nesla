#pragma once

#include "common.h"
#include "nes.h"
#include "mmu.h"

void cpu_step(struct nes *nes);
void cpu_at_power_up(struct nes *nes);
