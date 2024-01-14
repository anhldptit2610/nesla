#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "nes.h"
#include "mapper.h"

void cart_load(struct nes *nes, char *rom_path);
void cart_print_info(struct rom_info *info);
int cart_parse_header(struct nes *nes, uint8_t *header);
void cart_unload(struct nes *nes);
void cart_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode);

#ifdef __cplusplus
}
#endif