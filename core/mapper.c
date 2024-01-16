#include "mapper.h"

enum SUPPORTED_MAPPER {
    MAPPER_000
};

static void m000_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    addr = (nes->cart.info.prg_size == 16384) ? addr & 0xbfff : addr;
    if (mode == READ)
        *val = nes->cart.prg_rom[addr - 0x8000];
}

void (*mapper_handler[])(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode) = {
    [MAPPER_000] = m000_rw
};

void mapper_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    mapper_handler[nes->cart.info.mapper](nes, addr, val, mode);
}