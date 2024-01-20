#include "ppu.h"

enum PPU_REGISTERS {
    PPUCTRL = 0x2000,
    PPUMASK = 0x2001,
    PPUSTATUS = 0x2002,
    OAMADDR = 0x2003,
    OAMDATA = 0x2004,
    PPUSCROLL = 0x2005,
    PPUADDR = 0x2006,
    PPUDATA = 0x2007,
};

enum SCANLINE_STAGE {
    IDLE = 1,
    GET_TILE_DATA = 2,
    GET_SPRITE_DATA = 4,
    GET_TWO_TILES_NEXT_LINE = 8,
    DUMMY_FETCH = 16,
};

/* Nametable region based on the return value of this function
   
   1 -> $2000 - $23ff
   2 -> $2400 - $27ff
   4 -> $2800 - $2bff
   8 -> $2c00 - $2fff
*/
static int get_nametable_region(uint16_t addr)
{
    return (((addr >= 0x2000 && addr <= 0x23ff) << 0) |
            ((addr >= 0x2400 && addr <= 0x27ff) << 1) |
            ((addr >= 0x2800 && addr <= 0x2bff) << 2) |
            ((addr >= 0x2c00 && addr <= 0x2fff) << 3));
}

static void vram_io(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    switch (nes->cart.info.mirroring) {
    case HORIZONTAL:
        if (get_nametable_region(addr) == 1 || get_nametable_region(addr) == 2)
            addr = (addr & 0x23ff) - 0x2000;
        else if (get_nametable_region(addr) == 4 || get_nametable_region(addr) == 8)
            addr = (addr & 0x2bff) - 0x2400;
        break;
    case VERTICAL:
        if (get_nametable_region(addr) == 1 || get_nametable_region(addr) == 3)
            addr = (addr & 0x23ff) - 0x2000;
        else if (get_nametable_region(addr) == 2 || get_nametable_region(addr) == 4)
            addr = (addr & 0x27ff) - 0x2000;
    default:
        break;
    }

    switch (mode) {
    case READ:
        nes->ppu.vram[addr] = *val;
        break;
    case WRITE:
        *val = nes->ppu.vram[addr];
        break; 
    default:
        break;
    }
}

/* PPU memory map

   Address range   | Size    | Description
   $0000-$0fff     | $1000   | Pattern table 0
   $1000-$1fff     | $1000   | Pattern table 1
   $2000-$23ff     | $0400   | Nametable 0
   $2400-$27ff     | $0400   | Nametable 1
   $2800-$2bff     | $0400   | Nametable 2
   $2c00-$2fff     | $0400   | Nametable 3
   $3000-$3eff     | $0f00   | Mirrors of $2000-$2eff 
   $3f00-$3f1f     | $0020   | Palette RAM indexes 
   $3f20-$3fff     | $00e0   | Mirrors of $3f00-$3f1f 

*/

static void mem_io(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    addr = (addr >= 0x3000 && addr <= 0x3eff) ? addr & 0x2eff : addr;
    addr = (addr >= 0x3f20 && addr <= 0x3fff) ? addr & 0x3f1f : addr;

    if (addr >= 0x2000 && addr <= 0x2c00) {
        vram_io(nes, addr, val, mode);
    } else {
        switch (mode) {
        case READ:
            if (addr >= 0x3f00 && addr <= 0x3f1f) {
                *val = nes->ppu.mem[addr];
            } else {
                *val = nes->ppu.read_buffer;
                nes->ppu.read_buffer = nes->ppu.mem[addr];
            }
            break;
        case WRITE:
            nes->ppu.mem[addr] = nes->ppu.io_db = *val;
            break;
        default:
            break;
        }
    }
}

void ppu_read(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    switch (addr) {
    case PPUSTATUS:
        nes->ppu.io_db = (nes->ppu.io_db & 0x1f) | (nes->ppu.ppustatus & 0x60) |
                            ((uint8_t)nes->ppu.nmi_occured << 7);
        nes->ppu.w = 0;
        nes->ppu.nmi_occured = false;
        break;
    case OAMDATA:
        nes->ppu.io_db = nes->ppu.oam[nes->ppu.oamaddr];
        break;
    case PPUDATA:
        mem_io(nes, nes->ppu.v, &nes->ppu.io_db, READ);
        nes->ppu.v += (nes->ppu.I) ? 32 : 1;
        break;
    default:
        break;
    }
}

void ppu_write(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    switch (addr) {
    case PPUCTRL:
        nes->ppu.ppuctrl = nes->ppu.io_db = *val;
        nes->ppu.nmi_output = nes->ppu.ppuctrl & 0x80;
        if (nes->ppu.nmi_occured && nes->ppu.nmi_output)
            nes->cpu.nmi = 0;

        nes->ppu.t = (nes->ppu.t & 0x73ff) | ((uint16_t)(*val & 0x03) << 10);
        break;
    case PPUMASK:
        nes->ppu.ppumask = nes->ppu.io_db = *val; 
        break;
    case OAMADDR:
        nes->ppu.oamaddr = nes->ppu.io_db = *val;
        break;
    case OAMDATA:
        nes->ppu.oam[nes->ppu.oamaddr++] = nes->ppu.io_db = *val;
        break;
    case PPUSCROLL:
        if (!nes->ppu.w) {
            nes->ppu.t = (nes->ppu.t & 0x7fe0) | (*val >> 3);
            nes->ppu.x = *val & 0x07;
            nes->ppu.w = 1;
        } else if (nes->ppu.w == 1) {
            nes->ppu.t = (nes->ppu.t & 0x0c1f) | (uint16_t)(*val & 0x07) << 12 |
                            (uint16_t)(*val >> 3) << 5;
            nes->ppu.w = 0;
        }
        nes->ppu.io_db = *val;
        break;
    case PPUADDR:
        if (!nes->ppu.w) {
            nes->ppu.t = (nes->ppu.t & 0x00ff) | (uint16_t)(*val & 0x3f) << 8;
            nes->ppu.w++;
        } else if (nes->ppu.w == 1) {
            nes->ppu.t = (nes->ppu.t & 0x7f00) | (uint16_t)(*val);
            nes->ppu.v = nes->ppu.t;
            nes->ppu.w = 0;
        }
        nes->ppu.io_db = *val;
        break;
    case PPUDATA:
        mem_io(nes, nes->ppu.v, val, WRITE);
        nes->ppu.v += (!nes->ppu.I) ? 1 : 32;
        break;
    default:
        nes->ppu.io_db = *val;
        break;
    }
}

void (*callback[])(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode) = {
    [READ] = ppu_read,
    [WRITE] = ppu_write,
};

void ppu_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    addr &= 0x2007;
    callback[mode](nes, addr, val, mode);
    *val = nes->ppu.io_db;
}

static int get_cycle_stage(int cycles)
{
    return (((!cycles) << 0) |
           ((cycles >= 1 && cycles <= 256) << 1) |
           ((cycles >= 257 && cycles <= 320) << 2) |
           ((cycles >= 321 && cycles <= 336) << 3) |
           ((cycles >= 337 && cycles <= 340) << 4));
}

void ppu_tick(struct nes *nes)
{
    switch (get_cycle_stage(nes->ppu.cycles)) {
    case IDLE:
        break;
    case GET_TILE_DATA:
        if (nes->ppu.cycles == 1 && nes->ppu.scanlines == 241) {
            nes->ppu.VBL = 1;
            nes->ppu.nmi_occured = true;
        } else if (nes->ppu.cycles == 1 && nes->ppu.scanlines == 261) {
            nes->ppu.VBL = 0;
            nes->ppu.nmi_occured = false;
        }
        break;
    case GET_SPRITE_DATA:
        break;
    case GET_TWO_TILES_NEXT_LINE:
        break;
    case DUMMY_FETCH:
        break; 
    default:
        break;
    }
    if (nes->ppu.cycles == 340) {
        if (nes->ppu.scanlines == 261) {
            nes->ppu.scanlines = 0;
        } else {
            nes->ppu.scanlines++;
        }
        nes->ppu.cycles = 0;
    } else {
        nes->ppu.cycles++;
    }
}

void ppu_at_power_up(struct nes *nes)
{
    nes->ppu.cycles = 0;
    nes->ppu.scanlines = 0;
}