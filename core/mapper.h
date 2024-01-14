#pragma once

#ifdef __cplusplus
    extern "C" {
#endif

#include "nes.h"

void mapper_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode);

#ifdef __cplusplus
    }
#endif