#include "mmu.h"

uint8_t mmu_read(struct nes *nes, uint16_t addr)
{
    return nes->mem[addr];
}

void mmu_write(struct nes *nes, uint16_t addr, uint8_t val)
{
    nes->mem[addr] = val;
}