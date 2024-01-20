#include "cpu.h"

void cache_push(struct nes *nes, uint16_t addr)
{
    nes->instr_addr_cache[nes->cache_index] = addr;
    nes->cache_index = (nes->cache_index + 1) % CACHE_SIZE;
    if (nes->cache_size < 6)
        nes->cache_size++;
}

uint16_t cache_get_nth_element(struct nes *nes, int n)
{
    if (n > nes->cache_size)
        return 0;
    int i = nes->cache_index - n;
    if (i < 0)
        i += CACHE_SIZE;  
    return nes->instr_addr_cache[i];
}

/* cycle/tick functions */
void cpu_cycle(struct nes *nes)
{
    ppu_tick(nes);
    ppu_tick(nes);
    ppu_tick(nes);
}

/* The NMI input is connected to an edge detector, the IRQ input is connected
   to the level detector. Edge detector polls the status of NMI line during 
   bottom half of each CPU cycle, raises an internal signal if the input goes
   from high during one cycle to low during next. Level detector poll the status
   of IRQ line during bottom half. They both raise the internal signal high during
   the next top half the the following cycle, but IRQ only high for that cycle only.
*/

void cycle_top_half(struct nes *nes)
{
    nes->cpu.prev_nmi = nes->cpu.nmi;
    nes->cpu.prev_irq = nes->cpu.irq;


    if (nes->cpu.raise_nmi && !nes->cpu.nmi_pending)
        nes->cpu.nmi_pending = 1;
    nes->cpu.raise_nmi = 0;

    // IRQ internal signal goes high for 1 cycle only
    nes->cpu.irq_pending = (nes->cpu.raise_irq) ? 1 : 0;
    nes->cpu.raise_irq = 0;
}

void cycle_bottom_half(struct nes *nes)
{
    nes->cpu.raise_nmi = (nes->cpu.prev_nmi && !nes->cpu.nmi) ? 1 : 0;
    nes->cpu.raise_irq = (!nes->cpu.irq) ? 1 : 0;
}

/* cpu read/write functions */
uint8_t cpu_read(struct nes *nes, uint16_t addr)
{
    uint8_t ret;

    cycle_top_half(nes);
    cpu_cycle(nes);
    ret =  mmu_read(nes, addr);
    cycle_bottom_half(nes);
    return ret;
}

void cpu_write(struct nes *nes, uint16_t addr, uint8_t val)
{
    cycle_top_half(nes);
    cpu_cycle(nes);
    mmu_write(nes, addr, val);
    cycle_bottom_half(nes);
}

static uint8_t fetch_8(struct nes *nes)
{
    return cpu_read(nes, nes->cpu.pc++);
}

static uint16_t fetch_16(struct nes *nes)
{
    uint8_t lb = cpu_read(nes, nes->cpu.pc++);
    uint8_t hb = cpu_read(nes, nes->cpu.pc++);

    return TO_U16(lb, hb);
}

void stack_push_8(struct nes *nes, uint8_t data)
{
    cpu_write(nes, STACK_BASE | nes->cpu.sp, data);
    nes->cpu.sp--;
}

void stack_push_16(struct nes *nes, uint16_t data)
{
    stack_push_8(nes, MSB(data));
    stack_push_8(nes, LSB(data));
}

uint8_t stack_pop_8(struct nes *nes)
{
    nes->cpu.sp++;
    return cpu_read(nes, STACK_BASE + nes->cpu.sp);
}

uint16_t stack_pop_16(struct nes *nes)
{
    uint8_t pcl, pch;

    pcl = stack_pop_8(nes);
    pch = stack_pop_8(nes);
    return TO_U16(pcl, pch);
}

/* instructions - related */

typedef enum CPU {
    C,
    Z,
    I,
    D,
    B,
    U,
    V,
    N,
} cpu_flag_t;

typedef struct instruction {
    char name[20];
    addr_mode_t addr_mode;
    void (*handler)(struct nes *, addr_mode_t mode);
} instruction_t;

/* Load/Store Operations */

static void lda(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        nes->cpu.a = nes->cpu.operation_value;
        break;
    case ZP:
    case ZPX:
    case ZPY:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    }

    nes->cpu.a = nes->cpu.operation_value;

    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.N = BIT(nes->cpu.a, 7);
}

static void ldx(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPY:
    case ABS:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        break;
    case ABSY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    }

    nes->cpu.x = nes->cpu.operation_value;

    nes->cpu.Z = !nes->cpu.x;
    nes->cpu.N = BIT(nes->cpu.x, 7);
}

static void ldy(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        break;
    case ABSX:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    }

    nes->cpu.y = nes->cpu.operation_value;

    nes->cpu.Z = !nes->cpu.y;
    nes->cpu.N = BIT(nes->cpu.y, 7);
}

static void sta(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.a);
        break;
    default:
        break;
    }
}

static void stx(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPY:
    case ABS:
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.x);
        break;
    default:
        break;
    }
}

static void sty(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.y);
        break;
    default:
        break;
    }
}

/* Register Transfers */

static void tay(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.y = nes->cpu.a;

    nes->cpu.Z = !nes->cpu.y;
    nes->cpu.N = BIT(nes->cpu.y, 7);
}

static void tax(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.x = nes->cpu.a;

    nes->cpu.Z = !nes->cpu.x;
    nes->cpu.N = BIT(nes->cpu.x, 7);
}

static void txa(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.a = nes->cpu.x;

    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.N = BIT(nes->cpu.a, 7);
}

static void tya(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.a = nes->cpu.y;

    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.N = BIT(nes->cpu.a, 7);
}

/* Stack Operations */

static void tsx(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.x = nes->cpu.sp;

    nes->cpu.Z = !nes->cpu.x;
    nes->cpu.N = BIT(nes->cpu.x, 7);
}

static void txs(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.sp = nes->cpu.x;
}

static void pha(struct nes *nes, addr_mode_t mode)
{
    stack_push_8(nes, nes->cpu.a);
}

static void php(struct nes *nes, addr_mode_t mode)
{
    stack_push_8(nes, nes->cpu.p | 0x30);
}

static void pla(struct nes *nes, addr_mode_t mode)
{
    cpu_read(nes, STACK_BASE + nes->cpu.sp);
    nes->cpu.a = stack_pop_8(nes);

    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.N = BIT(nes->cpu.a, 7);
}

static void plp(struct nes *nes, addr_mode_t mode)
{
    cpu_read(nes, STACK_BASE + nes->cpu.sp);
    nes->cpu.p = (nes->cpu.p & 0x30) | (stack_pop_8(nes) & 0xcf);
}

/* Logical */

static void and(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr); 
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    }

    nes->cpu.a &= nes->cpu.operation_value;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
}

static void eor(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr); 
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    } 

    nes->cpu.a ^= nes->cpu.operation_value;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
}

static void ora(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr); 
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    }

    nes->cpu.a |= nes->cpu.operation_value;
    
    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
}

static void bit(struct nes *nes, addr_mode_t mode)
{
    uint8_t memory;

    switch (mode) {
    case ZP:
    case ABS:
        memory = cpu_read(nes, nes->cpu.effective_addr);

        nes->cpu.Z = !(nes->cpu.a & memory);
        nes->cpu.N = BIT(memory, 7);
        nes->cpu.V = BIT(memory, 6);
        break;
    default:
        break;
    }
}

/* Arithmetic */

static void adc(struct nes *nes, addr_mode_t mode)
{
    uint8_t a, m, c;

    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr); 
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break; 
    default:
        break;
    }

    a = nes->cpu.a;
    c = BIT(nes->cpu.p, C);
    m = nes->cpu.operation_value;
    nes->cpu.a = a + m + c;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.V = ((a ^ nes->cpu.a) & (m ^ nes->cpu.a) & 0x80) > 0;
    nes->cpu.C = ((a + m + c) & 0x100) > 0;
}

static void sbc(struct nes *nes, addr_mode_t mode)
{
    uint8_t a, c, m;

    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr); 
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break; 
    default:
        break;
    }

    a = nes->cpu.a;
    c = BIT(nes->cpu.p, C);
    m = nes->cpu.operation_value ^ 0xff;
    nes->cpu.a += (nes->cpu.operation_value ^ 0xff) + c;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.V = ((a ^ nes->cpu.a) & (m ^ nes->cpu.a) & 0x80) > 0;
    nes->cpu.C = ((a + m + c) & 0x100) > 0;
}

static void cmp(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ZPX:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr); 
        break;
    case ABSX:
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break; 
    default:
        break;
    }

    nes->cpu.N = BIT(nes->cpu.a - nes->cpu.operation_value, 7);
    nes->cpu.Z = !(nes->cpu.a - nes->cpu.operation_value);
    nes->cpu.C = nes->cpu.a >= nes->cpu.operation_value;
}

static void cpx(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ABS:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);    
        break;
    default:
        break;
    }

    nes->cpu.N = BIT(nes->cpu.x - nes->cpu.operation_value, 7);
    nes->cpu.Z = !(nes->cpu.x - nes->cpu.operation_value);
    nes->cpu.C = nes->cpu.x >= nes->cpu.operation_value;
}

static void cpy(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case IMM:
        break;
    case ZP:
    case ABS:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);    
        break;
    default:
        break;
    }

    nes->cpu.N = BIT(nes->cpu.y - nes->cpu.operation_value, 7);
    nes->cpu.Z = !(nes->cpu.y - nes->cpu.operation_value);
    nes->cpu.C = nes->cpu.y >= nes->cpu.operation_value;
}

/* Increments & Decrements */

static void inc(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        break;
    default:
        break;
    }

    cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
    cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value + 1);

    nes->cpu.N = BIT(nes->cpu.operation_value + 1, 7);
    nes->cpu.Z = !((nes->cpu.operation_value + 1) & 0xff);
}

static void inx(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.x += 1;

    nes->cpu.N = BIT(nes->cpu.x, 7);
    nes->cpu.Z = !nes->cpu.x;
}

static void iny(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.y += 1;

    nes->cpu.N = BIT(nes->cpu.y, 7);
    nes->cpu.Z = !nes->cpu.y;
}

static void dec(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        break;
    default:
        break;
    }

    cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
    cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value - 1);

    nes->cpu.N = BIT(nes->cpu.operation_value - 1, 7);
    nes->cpu.Z = !((nes->cpu.operation_value - 1) & 0xff);
}

static void dex(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.x -= 1;

    nes->cpu.N = BIT(nes->cpu.x, 7);
    nes->cpu.Z = !nes->cpu.x;
}

static void dey(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.y -= 1;

    nes->cpu.N = BIT(nes->cpu.y, 7);
    nes->cpu.Z = !nes->cpu.y;
}

/* shifts */

static void asl(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c, tmp;

    switch (mode) {
    case ACC:
        new_c = BIT(nes->cpu.a, 7);
        nes->cpu.a <<= 1;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
        tmp = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, tmp);
        new_c = BIT(tmp, 7);
        tmp <<= 1;
        cpu_write(nes, nes->cpu.effective_addr, tmp);

        nes->cpu.N = BIT(tmp, 7);
        nes->cpu.Z = !tmp;
        nes->cpu.C = new_c > 0;
    default:
        break;
    }
}

static void lsr(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c, tmp;

    switch (mode) {
    case ACC:
        new_c = BIT(nes->cpu.a, 0);
        nes->cpu.a >>= 1;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
        tmp = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, tmp);
        new_c = BIT(tmp, 0);
        tmp >>= 1;
        cpu_write(nes, nes->cpu.effective_addr, tmp);

        nes->cpu.N = BIT(tmp, 7);
        nes->cpu.Z = !tmp;
        nes->cpu.C = new_c > 0;
    default:
        break;
    }
}

static void rol(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c, old_c, tmp;

    old_c = BIT(nes->cpu.p, C);
    switch (mode) {
    case ACC:
        new_c = BIT(nes->cpu.a, 7);
        nes->cpu.a = (nes->cpu.a << 1) | old_c;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
        tmp = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, tmp);
        new_c = BIT(tmp, 7);
        tmp = (tmp << 1) | old_c;
        cpu_write(nes, nes->cpu.effective_addr, tmp);

        nes->cpu.N = BIT(tmp, 7);
        nes->cpu.Z = !tmp;
        nes->cpu.C = new_c > 0;
    default:
        break;
    }
}

static void ror(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c, tmp, old_c;

    old_c = BIT(nes->cpu.p, C);
    switch (mode) {
    case ACC:
        new_c = BIT(nes->cpu.a, 0);
        nes->cpu.a = (nes->cpu.a >> 1) | (old_c << 7);

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
        tmp = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, tmp);
        new_c = BIT(tmp, C);
        tmp = (tmp >> 1) | (old_c << 7);
        cpu_write(nes, nes->cpu.effective_addr, tmp);

        nes->cpu.N = BIT(tmp, 7);
        nes->cpu.Z = !tmp;
        nes->cpu.C = new_c > 0;
    default:
        break;
    }
}

/* Jumps & Calls */
static void jmp(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.pc = nes->cpu.effective_addr;
}

static void jsr(struct nes *nes, addr_mode_t mode)
{
    cpu_read(nes, STACK_BASE + nes->cpu.sp);
    stack_push_16(nes, nes->cpu.pc);
    nes->cpu.pc = TO_U16(nes->cpu.operand[0], fetch_8(nes));
}

static void rts(struct nes *nes, addr_mode_t mode)
{
    cpu_read(nes, STACK_BASE + nes->cpu.sp);
    nes->cpu.pc = stack_pop_16(nes);
    cpu_read(nes, nes->cpu.pc++);
}

/* Branches */

static void bcc(struct nes *nes, addr_mode_t mode)
{
    if (!BIT(nes->cpu.p, C)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void bcs(struct nes *nes, addr_mode_t mode)
{
    if (BIT(nes->cpu.p, C)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void beq(struct nes *nes, addr_mode_t mode)
{
    if (BIT(nes->cpu.p, Z)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void bmi(struct nes *nes, addr_mode_t mode)
{
    if (BIT(nes->cpu.p, N)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void bne(struct nes *nes, addr_mode_t mode)
{
    if (!BIT(nes->cpu.p, Z)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void bpl(struct nes *nes, addr_mode_t mode)
{
    if (!BIT(nes->cpu.p, N)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void bvc(struct nes *nes, addr_mode_t mode)
{
    if (!BIT(nes->cpu.p, V)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

static void bvs(struct nes *nes, addr_mode_t mode)
{
    if (BIT(nes->cpu.p, V)) {
        cpu_read(nes, nes->cpu.pc);
        nes->cpu.pc = nes->cpu.non_effective_addr;
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            cpu_read(nes, nes->cpu.pc);
            nes->cpu.pc = nes->cpu.effective_addr;
        }
    }
}

/* Status Flag Changes */

static void clc(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.C = 0;
}

static void cld(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.D = 0;
}

static void cli(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.I = 0;
}

static void clv(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.V = 0;
}

static void sec(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.C = 1;
}

static void sed(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.D = 1;
}

static void sei(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.I = 1;
}

/* System Functions */

static void brk(struct nes *nes, addr_mode_t mode)
{
    uint8_t pcl, pch;
    uint16_t base_addr;

    nes->cpu.pc++;
    stack_push_16(nes, nes->cpu.pc);
    base_addr = (nes->cpu.nmi_pending) ? NMI_VECTOR_BASE : IRQ_BRK_VECTOR_BASE;
    stack_push_8(nes, nes->cpu.p | 0x10);
    nes->cpu.I = 1;
    pcl = cpu_read(nes, base_addr);
    pch = cpu_read(nes, base_addr + 1);
    nes->cpu.pc = TO_U16(pcl, pch);
}

static void nop(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ABS:
    case ZPX:
    case ABSX:
        cpu_read(nes, nes->cpu.effective_addr);
        break;
    default:
        break;
    }
}

static void rti(struct nes *nes, addr_mode_t mode)
{
    cpu_read(nes, STACK_BASE + nes->cpu.sp);
    nes->cpu.p = (nes->cpu.p & 0x30) | (stack_pop_8(nes) & 0xcf);
    nes->cpu.pc = stack_pop_16(nes);
}

/* Unofficial opcodes */

static void sax(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPY:
    case ABS:
    case XIND:
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.a & nes->cpu.x);
        break;
    default:
        break;
    }
}

static void jam(struct nes *nes, addr_mode_t mode)
{
    // freeze the CPU ???
    nes->cpu.pc--;
}

static void slo(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c;

    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        new_c = BIT(nes->cpu.operation_value, 7);
        nes->cpu.operation_value <<= 1;
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        nes->cpu.a |= nes->cpu.operation_value;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    default:
        break;
    }
}

static void anc(struct nes *nes, addr_mode_t mode)
{
    nes->cpu.a = nes->cpu.a & nes->cpu.operation_value;

    nes->cpu.N = nes->cpu.C = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.C = BIT(nes->cpu.a, 7);
}

static void rla(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c, old_c;

    old_c = BIT(nes->cpu.p, C);
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        new_c = BIT(nes->cpu.operation_value, 7);
        nes->cpu.operation_value = (nes->cpu.operation_value << 1) | old_c;
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        nes->cpu.a &= nes->cpu.operation_value;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    default:
        break;
    }
}

static void sre(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c;

    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        new_c = nes->cpu.operation_value & 0x01;
        new_c = BIT(nes->cpu.operation_value, 0);
        nes->cpu.operation_value >>= 1;
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        nes->cpu.a ^= nes->cpu.operation_value;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = new_c > 0;
        break;
    default:
        break;
    }
}

static void alr(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c;

    nes->cpu.a = nes->cpu.a & nes->cpu.operation_value;
    new_c = BIT(nes->cpu.a, 0);
    nes->cpu.a >>= 1;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.C = new_c > 0;
}

static void rra(struct nes *nes, addr_mode_t mode)
{
    uint8_t new_c, old_c, a, b;

    old_c = BIT(nes->cpu.p, C);
    a = nes->cpu.a;
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        new_c = BIT(nes->cpu.operation_value, 0);
        nes->cpu.operation_value = b = (nes->cpu.operation_value >> 1) | (old_c << 7);
        // ADC oper
        nes->cpu.a += b + new_c;
        cpu_write(nes, nes->cpu.effective_addr, b);

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = ((a + b + new_c) & 0x100) > 0;
        nes->cpu.V = ((a ^ nes->cpu.a) & (b ^ nes->cpu.a) & 0x80) > 0;
        break;
    default:
        break;
    }
}

static void arr(struct nes *nes, addr_mode_t mode)
{
    uint8_t c;

    c = BIT(nes->cpu.p, C);

    nes->cpu.a &= nes->cpu.operation_value;
    nes->cpu.a = (nes->cpu.a >> 1) | (c << 7);

    nes->cpu.Z = !nes->cpu.a;
    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.C = BIT(nes->cpu.a, 6);
    nes->cpu.V = BIT(nes->cpu.a, 6) ^ BIT(nes->cpu.a, 5);
}

static void ane(struct nes *nes, addr_mode_t mode)
{
    // highly unstable 
}

static void sha(struct nes *nes, addr_mode_t mode)
{
    // unstable
}

static void tas(struct nes *nes, addr_mode_t mode)
{
    // unstable
}

static void shy(struct nes *nes, addr_mode_t mode)
{
    // unstable
}

static void shx(struct nes *nes, addr_mode_t mode)
{
    // unstable
}

static void lax(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPY:
    case ABS:
    case XIND:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        break;
    case ABSY:
    case INDY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        } 
        break;
    default:
        break;
    }

    nes->cpu.a = nes->cpu.x = nes->cpu.operation_value;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
}

static void lxa(struct nes *nes, addr_mode_t mode)
{
    // highly unstable
}

static void las(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ABSY:
        if (nes->cpu.page_boundary_crossed) {
            nes->cpu.page_boundary_crossed = false;
            nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        }
        break;
    default:
        break;
    }

    nes->cpu.a = nes->cpu.x = nes->cpu.sp = nes->cpu.operation_value & nes->cpu.sp;

    nes->cpu.N = BIT(nes->cpu.a, 7);
    nes->cpu.Z = !nes->cpu.a;
}

static void dcp(struct nes *nes, addr_mode_t mode)
{
    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        nes->cpu.operation_value -= 1;
        nes->cpu.N = BIT(nes->cpu.a - nes->cpu.operation_value, 7);
        nes->cpu.Z = !(nes->cpu.a - nes->cpu.operation_value);
        nes->cpu.C = nes->cpu.a >= nes->cpu.operation_value;
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        break;
    default:
        break;
    }
}

static void sbx(struct nes *nes, addr_mode_t mode)
{
    uint8_t a = nes->cpu.a & nes->cpu.x, b = nes->cpu.operation_value;

    nes->cpu.x = (nes->cpu.a & nes->cpu.x) - nes->cpu.operation_value;

    nes->cpu.N = BIT(nes->cpu.x, 7);
    nes->cpu.Z = !nes->cpu.x;
    nes->cpu.C = a >= b;
}

static void isc(struct nes *nes, addr_mode_t mode)
{
    uint8_t a = nes->cpu.a, tmp, c = BIT(nes->cpu.p, C);

    switch (mode) {
    case ZP:
    case ZPX:
    case ABS:
    case ABSX:
    case ABSY:
    case XIND:
    case INDY:
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.effective_addr);
        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);

        nes->cpu.operation_value += 1;
        tmp = nes->cpu.operation_value ^ 0xff;
        nes->cpu.a = a + tmp + c;

        nes->cpu.N = BIT(nes->cpu.a, 7);
        nes->cpu.Z = !nes->cpu.a;
        nes->cpu.C = ((a + tmp + c) & 0x100) > 0;
        nes->cpu.V = ((a ^ nes->cpu.a) & (tmp ^ nes->cpu.a) & 0x80) > 0;

        cpu_write(nes, nes->cpu.effective_addr, nes->cpu.operation_value);
        break;
    default:
        break;
    }
}

static void usbc(struct nes *nes, addr_mode_t mode)
{
    sbc(nes, IMM);
}

// TODO: reformat opcode_table to make it looks nicer
instruction_t opcode_table[] = {
    [0x00] = {"BRK",  IMPL, brk},
    [0x01] = {"ORA", XIND, ora},
    [0x02] = {"JAM",       NONE, jam},
    [0x03] = {"SLO", XIND, slo},
    [0x04] = {"NOP",   ZP,   nop},
    [0x05] = {"ORA",   ZP,   ora},
    [0x06] = {"ASL",   ZP,   asl},
    [0x07] = {"SLO",   ZP,   slo},
    [0x08] = {"PHP",  IMPL, php},
    [0x09] = {"ORA",     IMM,  ora},
    [0x0a] = {"ASL",     ACC,  asl},
    [0x0b] = {"ANC",     IMM,  anc},
    [0x0c] = {"NOP",   ABS,  nop},
    [0x0d] = {"ORA",   ABS,  ora},
    [0x0e] = {"ASL",   ABS,  asl},
    [0x0f] = {"SLO",   ABS,  slo},
    [0x10] = {"BPL",   REL,  bpl},
    [0x11] = {"ORA", INDY, ora},
    [0x12] = {"JAM",       NONE, jam},
    [0x13] = {"SLO", INDY, slo},
    [0x14] = {"NOP", ZPX,  nop},
    [0x15] = {"ORA", ZPX,  ora},
    [0x16] = {"ASL", ZPX,  asl},
    [0x17] = {"SLO", ZPX,  slo},
    [0x18] = {"CLC",  IMPL, clc},
    [0x19] = {"ORA", ABSY, ora},
    [0x1a] = {"NOP",  IMPL, nop},
    [0x1b] = {"SLO", ABSY, slo},
    [0x1c] = {"NOP", ABSX, nop},
    [0x1d] = {"ORA", ABSX, ora},
    [0x1e] = {"ASL", ABSX, asl},
    [0x1f] = {"SLO", ABSX, slo},
    [0x20] = {"JSR",   ABS,  jsr},
    [0x21] = {"AND", XIND, and},
    [0x22] = {"JAM",       NONE, jam},
    [0x23] = {"RLA", XIND, rla},
    [0x24] = {"BIT",   ZP,   bit},
    [0x25] = {"AND",   ZP,   and},
    [0x26] = {"ROL",   ZP,   rol},
    [0x27] = {"RLA",   ZP,   rla},
    [0x28] = {"PLP",  IMPL, plp},
    [0x29] = {"AND",     IMM,  and},
    [0x2a] = {"ROL",     ACC,  rol},
    [0x2b] = {"ANC",     IMM,  anc},
    [0x2c] = {"BIT",   ABS,  bit},
    [0x2d] = {"AND",   ABS,  and},
    [0x2e] = {"ROL",   ABS,  rol},
    [0x2f] = {"RLA",   ABS,  rla},
    [0x30] = {"BMI",   REL,  bmi},
    [0x31] = {"AND", INDY, and},
    [0x32] = {"JAM",       NONE, jam},
    [0x33] = {"RLA", INDY, rla},
    [0x34] = {"NOP", ZPX,  nop},
    [0x35] = {"AND", ZPX,  and},
    [0x36] = {"ROL", ZPX,  rol},
    [0x37] = {"RLA", ZPX,  rla},
    [0x38] = {"SEC",  IMPL, sec},
    [0x39] = {"AND", ABSY, and},
    [0x3a] = {"NOP",  IMPL, nop},
    [0x3b] = {"RLA", ABSY, rla},
    [0x3c] = {"NOP", ABSX, nop},
    [0x3d] = {"AND", ABSX, and},
    [0x3e] = {"ROL", ABSX, rol},
    [0x3f] = {"RLA", ABSX, rla},
    [0x40] = {"RTI",  IMPL, rti},
    [0x41] = {"EOR", XIND, eor},
    [0x42] = {"JAM",       NONE, jam},
    [0x43] = {"SRE", XIND, sre},
    [0x44] = {"NOP",   ZP,   nop},
    [0x45] = {"EOR",   ZP,   eor},
    [0x46] = {"LSR",   ZP,   lsr},
    [0x47] = {"SRE",   ZP,   sre},
    [0x48] = {"PHA",  IMPL, pha},
    [0x49] = {"EOR",     IMM,  eor},
    [0x4a] = {"LSR",     ACC,  lsr},
    [0x4b] = {"ALR",     IMM,  alr},
    [0x4c] = {"JMP",   ABS,  jmp},
    [0x4d] = {"EOR",   ABS,  eor},
    [0x4e] = {"LSR",   ABS,  lsr},
    [0x4f] = {"SRE",   ABS,  sre},
    [0x50] = {"BVC",   REL,  bvc},
    [0x51] = {"EOR", INDY, eor},
    [0x52] = {"JAM",       NONE, jam},
    [0x53] = {"SRE", INDY, sre},
    [0x54] = {"NOP", ZPX,  nop},
    [0x55] = {"EOR", ZPX,  eor},
    [0x56] = {"LSR", ZPX,  lsr},
    [0x57] = {"SRE", ZPX,  sre},
    [0x58] = {"CLI",  IMPL, cli},
    [0x59] = {"EOR", ABSY, eor},
    [0x5a] = {"NOP",  IMPL, nop},
    [0x5b] = {"SRE", ABSY, sre},
    [0x5c] = {"NOP", ABSX, nop},
    [0x5d] = {"EOR", ABSX, eor},
    [0x5e] = {"LSR", ABSX, lsr},
    [0x5f] = {"SRE", ABSX, sre},
    [0x60] = {"RTS",  IMPL, rts},
    [0x61] = {"ADC", XIND, adc},
    [0x62] = {"JAM",       NONE, jam},
    [0x63] = {"RRA", XIND, rra},
    [0x64] = {"NOP",   ZP,   nop},
    [0x65] = {"ADC",   ZP,   adc},
    [0x66] = {"ROR",   ZP,   ror},
    [0x67] = {"RRA",   ZP,   rra},
    [0x68] = {"PLA",  IMPL, pla},
    [0x69] = {"ADC",     IMM,  adc},
    [0x6a] = {"ROR",     ACC,  ror},
    [0x6b] = {"ARR",     IMM,  arr},
    [0x6c] = {"JMP",   IND,  jmp},
    [0x6d] = {"ADC",   ABS,  adc},
    [0x6e] = {"ROR",   ABS,  ror},
    [0x6f] = {"RRA",   ABS,  rra},
    [0x70] = {"BVS",   REL,  bvs},
    [0x71] = {"ADC", INDY, adc},
    [0x72] = {"JAM",       NONE, jam},
    [0x73] = {"RRA", INDY, rra},
    [0x74] = {"NOP", ZPX,  nop},
    [0x75] = {"ADC", ZPX,  adc},
    [0x76] = {"ROR", ZPX,  ror},
    [0x77] = {"RRA", ZPX,  rra},
    [0x78] = {"SEI",  IMPL, sei},
    [0x79] = {"ADC", ABSY, adc},
    [0x7a] = {"NOP",  IMPL, nop},
    [0x7b] = {"RRA", ABSY, rra},
    [0x7c] = {"NOP", ABSX, nop},
    [0x7d] = {"ADC", ABSX, adc},
    [0x7e] = {"ROR", ABSX, ror},
    [0x7f] = {"RRA", ABSX, rra},
    [0x80] = {"NOP",     IMM,  nop},
    [0x81] = {"STA", XIND, sta},
    [0x82] = {"NOP",     IMM,  nop},
    [0x83] = {"SAX", XIND, sax},
    [0x84] = {"STY",   ZP,   sty},
    [0x85] = {"STA",   ZP,   sta},
    [0x86] = {"STX",   ZP,   stx},
    [0x87] = {"SAX",   ZP,   sax},
    [0x88] = {"DEY",  IMPL, dey},
    [0x89] = {"NOP",     IMM,  nop},
    [0x8a] = {"TXA",  IMPL, txa},
    [0x8b] = {"ANE",     IMM,  ane},
    [0x8c] = {"STY",   ABS,  sty},
    [0x8d] = {"STA",   ABS,  sta},
    [0x8e] = {"STX",   ABS,  stx},
    [0x8f] = {"SAX",   ABS,  sax},
    [0x90] = {"BCC",   REL,  bcc},
    [0x91] = {"STA", INDY, sta},
    [0x92] = {"JAM",       NONE, jam},
    [0x93] = {"SHA", INDY, sha},
    [0x94] = {"STY", ZPX,  sty},
    [0x95] = {"STA", ZPX,  sta},
    [0x96] = {"STX", ZPY,  stx},
    [0x97] = {"SAX", ZPY,  sax},
    [0x98] = {"TYA",  IMPL, tya},
    [0x99] = {"STA", ABSY, sta},
    [0x9a] = {"TXS",  IMPL, txs},
    [0x9b] = {"TAS", ABSY, tas},
    [0x9c] = {"SHY", ABSX, shy},
    [0x9d] = {"STA", ABSX, sta},
    [0x9e] = {"SHX", ABSY, shx},
    [0x9f] = {"SHA", ABSY, sha},
    [0xa0] = {"LDY",     IMM,  ldy},
    [0xa1] = {"LDA", XIND, lda},
    [0xa2] = {"LDX",     IMM,  ldx},
    [0xa3] = {"LAX", XIND, lax},
    [0xa4] = {"LDY",   ZP,   ldy},
    [0xa5] = {"LDA",   ZP,   lda},
    [0xa6] = {"LDX",   ZP,   ldx},
    [0xa7] = {"LAX",   ZP,   lax},
    [0xa8] = {"TAY",  IMPL, tay},
    [0xa9] = {"LDA",     IMM,  lda},
    [0xaa] = {"TAX",  IMPL, tax},
    [0xab] = {"LXA",     IMM,  lxa},
    [0xac] = {"LDY",   ABS,  ldy},
    [0xad] = {"LDA",   ABS,  lda},
    [0xae] = {"LDX",   ABS,  ldx},
    [0xaf] = {"LAX",   ABS,  lax},
    [0xb0] = {"BCS",   REL,  bcs},
    [0xb1] = {"LDA", INDY, lda},
    [0xb2] = {"JAM",       NONE, jam},
    [0xb3] = {"LAX", INDY, lax},
    [0xb4] = {"LDY", ZPX,  ldy},
    [0xb5] = {"LDA", ZPX,  lda},
    [0xb6] = {"LDX", ZPY,  ldx},
    [0xb7] = {"LAX", ZPY,  lax},
    [0xb8] = {"CLV",  IMPL, clv},
    [0xb9] = {"LDA", ABSY, lda},
    [0xba] = {"TSX",  IMPL, tsx},
    [0xbb] = {"LAS", ABSY, las},
    [0xbc] = {"LDY", ABSX, ldy},
    [0xbd] = {"LDA", ABSX, lda},
    [0xbe] = {"LDX", ABSY, ldx},
    [0xbf] = {"LAX", ABSY, lax},
    [0xc0] = {"CPY",     IMM,  cpy},
    [0xc1] = {"CMP", XIND, cmp},
    [0xc2] = {"NOP",     IMM,  nop},
    [0xc3] = {"DCP", XIND, dcp},
    [0xc4] = {"CPY",   ZP,   cpy},
    [0xc5] = {"CMP",   ZP,   cmp},
    [0xc6] = {"DEC",   ZP,   dec},
    [0xc7] = {"DCP",   ZP,   dcp},
    [0xc8] = {"INY",  IMPL, iny},
    [0xc9] = {"CMP",     IMM,  cmp},
    [0xca] = {"DEX",  IMPL, dex},
    [0xcb] = {"SBX",     IMM,  sbx},
    [0xcc] = {"CPY",   ABS,  cpy},
    [0xcd] = {"CMP",   ABS,  cmp},
    [0xce] = {"DEC",   ABS,  dec},
    [0xcf] = {"DCP",   ABS,  dcp},
    [0xd0] = {"BNE",   REL,  bne},
    [0xd1] = {"CMP", INDY, cmp},
    [0xd2] = {"JAM",       NONE, jam},
    [0xd3] = {"DCP", INDY, dcp},
    [0xd4] = {"NOP", ZPX,  nop},
    [0xd5] = {"CMP", ZPX,  cmp},
    [0xd6] = {"DEC", ZPX,  dec},
    [0xd7] = {"DCP", ZPX,  dcp},
    [0xd8] = {"CLD",  IMPL, cld},
    [0xd9] = {"CMP", ABSY, cmp},
    [0xda] = {"NOP",  IMPL, nop},
    [0xdb] = {"DCP", ABSY, dcp},
    [0xdc] = {"NOP", ABSX, nop},
    [0xdd] = {"CMP", ABSX, cmp},
    [0xde] = {"DEC", ABSX, dec},
    [0xdf] = {"DCP", ABSX, dcp},
    [0xe0] = {"CPX",     IMM,  cpx},
    [0xe1] = {"SBC", XIND, sbc},
    [0xe2] = {"NOP",     IMM,  nop},
    [0xe3] = {"ISC", XIND, isc},
    [0xe4] = {"CPX",   ZP,   cpx},
    [0xe5] = {"SBC",   ZP,   sbc},
    [0xe6] = {"INC",   ZP,   inc},
    [0xe7] = {"ISC",   ZP,   isc},
    [0xe8] = {"INX",  IMPL, inx},
    [0xe9] = {"SBC",     IMM,  sbc},
    [0xea] = {"NOP",  IMPL, nop},
    [0xeb] = {"USBC",    IMM,  usbc},
    [0xec] = {"CPX",   ABS,  cpx},
    [0xed] = {"SBC",   ABS,  sbc},
    [0xee] = {"INC",   ABS,  inc},
    [0xef] = {"ISC",   ABS,  isc},
    [0xf0] = {"BEQ",   REL,  beq},
    [0xf1] = {"SBC", INDY, sbc},
    [0xf2] = {"JAM",       NONE,  jam},
    [0xf3] = {"ISC", INDY, isc},
    [0xf4] = {"NOP", ZPX,   nop},
    [0xf5] = {"SBC", ZPX,   sbc},
    [0xf6] = {"INC", ZPX,   inc},
    [0xf7] = {"ISC", ZPX,   isc},
    [0xf8] = {"SED",  IMPL, sed},
    [0xf9] = {"SBC", ABSY,  sbc},
    [0xfa] = {"NOP",  IMPL, nop},
    [0xfb] = {"ISC", ABSY,  isc},
    [0xfc] = {"NOP", ABSX,  nop},
    [0xfd] = {"SBC", ABSX,  sbc},
    [0xfe] = {"INC", ABSX,  inc},
    [0xff] = {"ISC", ABSX,  isc},
};

/* other utils */
void cpu_get_opcode_info(char *ret, uint8_t opcode)
{
    strcpy(ret, opcode_table[opcode].name);
}

void handle_addressing_mode(struct nes *nes, addr_mode_t addr_mode)
{
    uint8_t operand_8, lb, hb;
    uint16_t operand_16;

    switch (addr_mode) {
    case IMPL:
        // cycle #2 - read next instruction byte
        cpu_read(nes, nes->cpu.pc);
        break;
    case ACC:
        // cycle #2 - read next instruction byte
        cpu_read(nes, nes->cpu.pc);
        break;
    case IMM:
        // cycle #2
        nes->cpu.operation_value = fetch_8(nes);
        break;
    case ABS:
        // the JSR instruction only read operand 1 before pushing PC 
        // to the stack.
        if (nes->cpu.opcode == 0x20)
            nes->cpu.operand[0] = fetch_8(nes);
        else
            nes->cpu.effective_addr = fetch_16(nes);
        break;
    case ZP:
        // cycle #2
        nes->cpu.effective_addr = fetch_8(nes);
        break;
    case ZPX:
        // cycle #2
        nes->cpu.operand[0] = fetch_8(nes);
        // cycle #3
        cpu_read(nes, nes->cpu.operand[0]);
        nes->cpu.effective_addr = (nes->cpu.operand[0] + nes->cpu.x) & 0x00ff;
        break;
    case ZPY:
        // cycle #2
        nes->cpu.operand[0] = fetch_8(nes);
        // cycle #3
        cpu_read(nes, nes->cpu.operand[0]);
        nes->cpu.effective_addr = (nes->cpu.operand[0] + nes->cpu.y) & 0x00ff;
        break;
    case ABSX:
        // cycle #2 & #3
        operand_16 = fetch_16(nes);
        nes->cpu.effective_addr = operand_16 + nes->cpu.x;
        nes->cpu.non_effective_addr = (nes->cpu.effective_addr & 0x00ff) | (operand_16 & 0xff00);
        if ((nes->cpu.effective_addr & 0xff00) != (operand_16 & 0xff00))
            nes->cpu.page_boundary_crossed = true;
        // cycle #4
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.non_effective_addr);
        break;
    case ABSY:
        // cycle #2 & #3
        operand_16 = fetch_16(nes);
        nes->cpu.effective_addr = operand_16 + nes->cpu.y;
        nes->cpu.non_effective_addr = (nes->cpu.effective_addr & 0x00ff) | (operand_16 & 0xff00);
        if ((nes->cpu.effective_addr & 0xff00) != (operand_16 & 0xff00))
            nes->cpu.page_boundary_crossed = true;
        // cycle #4
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.non_effective_addr);
        break;
    case REL:
        // cycle #2
        operand_8 = fetch_8(nes);
        nes->cpu.effective_addr = nes->cpu.pc + (int8_t)operand_8; 
        nes->cpu.non_effective_addr = (nes->cpu.effective_addr & 0x00ff) | (nes->cpu.pc & 0xff00);
        if ((nes->cpu.pc & 0xff00) != (nes->cpu.effective_addr & 0xff00))
            nes->cpu.page_boundary_crossed = true;
        break;
    case XIND:
        // cycle #2
        nes->cpu.operand[0] = fetch_8(nes);
        // cycle #3
        cpu_read(nes, nes->cpu.operand[0]);
        // cycle #4
        lb = cpu_read(nes, (nes->cpu.operand[0] + nes->cpu.x) & 0x00ff);
        // cycle #5
        hb = cpu_read(nes, (nes->cpu.operand[0] + nes->cpu.x + 1) & 0x00ff);
        nes->cpu.effective_addr = TO_U16(lb, hb);
        break;
    case INDY:
        // cycle #2
        nes->cpu.operand[0] = fetch_8(nes);
        // cycle #3
        lb = cpu_read(nes, nes->cpu.operand[0]);
        // cycle #4
        hb = cpu_read(nes, (nes->cpu.operand[0] + 1) & 0x00ff);
        nes->cpu.effective_addr = TO_U16(lb, hb) + nes->cpu.y;
        nes->cpu.non_effective_addr = (nes->cpu.effective_addr & 0x00ff) | (TO_U16(lb, hb) & 0xff00);
        if ((nes->cpu.effective_addr & 0xff00) != (TO_U16(lb, hb) & 0xff00))
            nes->cpu.page_boundary_crossed = true;
        // cycle #5
        nes->cpu.operation_value = cpu_read(nes, nes->cpu.non_effective_addr);
        break;
    case IND:
        // cycle #2 & #3
        operand_16 = fetch_16(nes);
        // cycle #4
        lb = cpu_read(nes, operand_16);
        // cycle #5
        hb = cpu_read(nes, ((operand_16 + 1) & 0x00ff) | (operand_16 & 0xff00));
        nes->cpu.effective_addr = TO_U16(lb, hb);
        break;
    default:
        break;
    }
}

void write_log(struct nes *nes)
{
    FILE *fp = fopen("log.txt", "a");

    if (!fp) {
        fprintf(stderr, "Can't open/create log file\n");
        return;
    }
    fprintf(fp, "%04X  A:%02X X:%02X Y:%02X P:%02X SP:%02X\n",
            nes->cpu.pc, nes->cpu.a, nes->cpu.x, nes->cpu.y, nes->cpu.p, nes->cpu.sp);
    fclose(fp);
}

void cpu_step(struct nes *nes)
{
    cache_push(nes, nes->cpu.pc);

    nes->cpu.opcode = fetch_8(nes);
    handle_addressing_mode(nes, opcode_table[nes->cpu.opcode].addr_mode);
    // run the opcode execution
    opcode_table[nes->cpu.opcode].handler(nes, opcode_table[nes->cpu.opcode].addr_mode);


    // interrupt polling 
    // TODO: branch instructions and interrupts
    interrupt_process(nes);
}

void cpu_at_power_up(struct nes *nes)
{
    nes->cpu.p = 0x24;
    nes->cpu.a = 0;
    nes->cpu.x = 0;
    nes->cpu.y = 0;
    nes->cpu.sp = 0xfd;

    nes->cpu.mem[0x4017] = 0x00;
    nes->cpu.mem[0x4015] = 0x00;
    memset(&nes->cpu.mem[0x4000], 0, 0x10);
    memset(&nes->cpu.mem[0x4010], 0, 0x04);

    nes->cache_index = 0;
    nes->cache_size = 0;

    // TODO: APU registers state

    // interrupt internal parts
    nes->cpu.irq_pending = 0;
    nes->cpu.nmi_pending = 0;
    nes->cpu.nmi = 1;
    nes->cpu.prev_nmi = 1;
    nes->cpu.irq = 1;
    nes->cpu.prev_irq = 1;
}

/* misc functions which serves other sub - components */
void get_opcode_name(uint8_t opcode, char *ret)
{
    strcpy(ret, opcode_table[opcode].name);
}

addr_mode_t get_opcode_mode(uint8_t opcode)
{
    return opcode_table[opcode].addr_mode;
}