#include "mmu.h"

/* memory i/o */
void mem_io(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    switch (mode) {
    case READ:
        *val = nes->mem[addr];
        break;
    case WRITE:
        nes->mem[addr] = *val;
        break;
    default:
        break;
    }
}

enum MEM_REGION {
    RAM = 1,
    PPU = 2,
    APU = 4,
    CART = 8
};

uint8_t get_mem_region(uint16_t addr)
{
    return (((addr >= 0x0000 && addr <= 0x1fff) << 0) |
            ((addr >= 0x2000 && addr <= 0x3fff) << 1) |
            ((addr >= 0x4000 && addr <= 0x401f) << 2) |
            ((addr >= 0x4020 && addr <= 0xffff) << 3));
}

void ram_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    addr &= 0x07ff;
    mem_io(nes, addr, val, mode);
}

void (*mem_callback[])(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode) = {
    [RAM]  = ram_rw,
    [PPU]  = ppu_rw,
    [APU]  = apu_rw,
    [CART] = cart_rw,
};

uint8_t mmu_read(struct nes *nes, uint16_t addr)
{
    uint8_t ret;

    mem_callback[get_mem_region(addr)](nes, addr, &ret, READ);
    return ret;
}

void mmu_write(struct nes *nes, uint16_t addr, uint8_t val)
{
    mem_callback[get_mem_region(addr)](nes, addr, &val, WRITE);
}