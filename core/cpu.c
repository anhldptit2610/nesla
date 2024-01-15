#include "cpu.h"

/* cycle/tick functions */
void cpu_cycle(struct nes *nes)
{
    // TODO: complete this function
}

/* cpu read/write functions */
uint8_t cpu_read(struct nes *nes, uint16_t addr)
{
    cpu_cycle(nes);
    return mmu_read(nes, addr);
}

void cpu_write(struct nes *nes, uint16_t addr, uint8_t val)
{
    cpu_cycle(nes);
    return mmu_write(nes, addr, val);
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

typedef enum ADDRESSING_MODE {
    IMPL,    /* implicit */
    ACC,    /* accumulator */
    IMM,    /* immediate */
    ZP,     /* zero page */
    ABS,    /* absolute */
    REL,    /* relative */
    IND,    /* indirect */
    ZPX,    /* zero page indexed x */
    ZPY,    /* zero page indexed y */
    ABSX,   /* absolute indexed x */
    ABSY,   /* absolute indexed y */
    XIND,   /* indexed x indirect  */
    INDY,   /* indirect indexed y */
    NONE,
} addr_mode_t;

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
    printf("%04x\n", nes->cpu.effective_addr);
    nes->cpu.pc = nes->cpu.effective_addr;
}

static void jsr(struct nes *nes, addr_mode_t mode)
{
    cpu_read(nes, STACK_BASE + nes->cpu.sp);
    stack_push_16(nes, nes->cpu.pc);
    nes->cpu.pc = TO_U16(nes->cpu.operand[0], cpu_read(nes, nes->cpu.pc));
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

    nes->cpu.pc++;
    stack_push_16(nes, nes->cpu.pc);
    stack_push_8(nes, nes->cpu.p | 0x10);
    nes->cpu.I = 1;
    pcl = cpu_read(nes, 0xfffe);
    pch = cpu_read(nes, 0xffff);
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

static instruction_t opcode_table[] = {
    [0x00] = {"BRK impl",  IMPL, brk},
    [0x01] = {"ORA X,ind", XIND, ora},
    [0x02] = {"JAM",       NONE, jam},
    [0x03] = {"SLO X,ind", XIND, slo},
    [0x04] = {"NOP zpg",   ZP,   nop},
    [0x05] = {"ORA zpg",   ZP,   ora},
    [0x06] = {"ASL zpg",   ZP,   asl},
    [0x07] = {"SLO zpg",   ZP,   slo},
    [0x08] = {"PHP impl",  IMPL, php},
    [0x09] = {"ORA #",     IMM,  ora},
    [0x0a] = {"ASL A",     ACC,  asl},
    [0x0b] = {"ANC #",     IMM,  anc},
    [0x0c] = {"NOP abs",   ABS,  nop},
    [0x0d] = {"ORA abs",   ABS,  ora},
    [0x0e] = {"ASL abs",   ABS,  asl},
    [0x0f] = {"SLO abs",   ABS,  slo},
    [0x10] = {"BPL rel",   REL,  bpl},
    [0x11] = {"ORA ind,Y", INDY, ora},
    [0x12] = {"JAM",       NONE, jam},
    [0x13] = {"SLO ind,Y", INDY, slo},
    [0x14] = {"NOP zpg,X", ZPX,  nop},
    [0x15] = {"ORA zpg,X", ZPX,  ora},
    [0x16] = {"ASL zpg,X", ZPX,  asl},
    [0x17] = {"SLO zpg,X", ZPX,  slo},
    [0x18] = {"CLC impl",  IMPL, clc},
    [0x19] = {"ORA abs,Y", ABSY, ora},
    [0x1a] = {"NOP impl",  IMPL, nop},
    [0x1b] = {"SLO abs,Y", ABSY, slo},
    [0x1c] = {"NOP abs,X", ABSX, nop},
    [0x1d] = {"ORA abs,X", ABSX, ora},
    [0x1e] = {"ASL abs,X", ABSX, asl},
    [0x1f] = {"SLO abs,X", ABSX, slo},
    [0x20] = {"JSR abs",   ABS,  jsr},
    [0x21] = {"AND X,ind", XIND, and},
    [0x22] = {"JAM",       NONE, jam},
    [0x23] = {"RLA X,ind", XIND, rla},
    [0x24] = {"BIT zpg",   ZP,   bit},
    [0x25] = {"AND zpg",   ZP,   and},
    [0x26] = {"ROL zpg",   ZP,   rol},
    [0x27] = {"RLA zpg",   ZP,   rla},
    [0x28] = {"PLP impl",  IMPL, plp},
    [0x29] = {"AND #",     IMM,  and},
    [0x2a] = {"ROL A",     ACC,  rol},
    [0x2b] = {"ANC #",     IMM,  anc},
    [0x2c] = {"BIT abs",   ABS,  bit},
    [0x2d] = {"AND abs",   ABS,  and},
    [0x2e] = {"ROL abs",   ABS,  rol},
    [0x2f] = {"RLA abs",   ABS,  rla},
    [0x30] = {"BMI rel",   REL,  bmi},
    [0x31] = {"AND ind,Y", INDY, and},
    [0x32] = {"JAM",       NONE, jam},
    [0x33] = {"RLA ind,Y", INDY, rla},
    [0x34] = {"NOP zpg,X", ZPX,  nop},
    [0x35] = {"AND zpg,X", ZPX,  and},
    [0x36] = {"ROL zpg,X", ZPX,  rol},
    [0x37] = {"RLA zpg,X", ZPX,  rla},
    [0x38] = {"SEC impl",  IMPL, sec},
    [0x39] = {"AND abs,Y", ABSY, and},
    [0x3a] = {"NOP impl",  IMPL, nop},
    [0x3b] = {"RLA abs,Y", ABSY, rla},
    [0x3c] = {"NOP abs,X", ABSX, nop},
    [0x3d] = {"AND abs,X", ABSX, and},
    [0x3e] = {"ROL abs,X", ABSX, rol},
    [0x3f] = {"RLA abs,X", ABSX, rla},
    [0x40] = {"RTI impl",  IMPL, rti},
    [0x41] = {"EOR X,ind", XIND, eor},
    [0x42] = {"JAM",       NONE, jam},
    [0x43] = {"SRE X,ind", XIND, sre},
    [0x44] = {"NOP zpg",   ZP,   nop},
    [0x45] = {"EOR zpg",   ZP,   eor},
    [0x46] = {"LSR zpg",   ZP,   lsr},
    [0x47] = {"SRE zpg",   ZP,   sre},
    [0x48] = {"PHA impl",  IMPL, pha},
    [0x49] = {"EOR #",     IMM,  eor},
    [0x4a] = {"LSR A",     ACC,  lsr},
    [0x4b] = {"ALR #",     IMM,  alr},
    [0x4c] = {"JMP abs",   ABS,  jmp},
    [0x4d] = {"EOR abs",   ABS,  eor},
    [0x4e] = {"LSR abs",   ABS,  lsr},
    [0x4f] = {"SRE abs",   ABS,  sre},
    [0x50] = {"BVC rel",   REL,  bvc},
    [0x51] = {"EOR ind,Y", INDY, eor},
    [0x52] = {"JAM",       NONE, jam},
    [0x53] = {"SRE ind,Y", INDY, sre},
    [0x54] = {"NOP zpg,X", ZPX,  nop},
    [0x55] = {"EOR zpg,X", ZPX,  eor},
    [0x56] = {"LSR zpg,X", ZPX,  lsr},
    [0x57] = {"SRE zpg,X", ZPX,  sre},
    [0x58] = {"CLI impl",  IMPL, cli},
    [0x59] = {"EOR abs,Y", ABSY, eor},
    [0x5a] = {"NOP impl",  IMPL, nop},
    [0x5b] = {"SRE abs,Y", ABSY, sre},
    [0x5c] = {"NOP abs,X", ABSX, nop},
    [0x5d] = {"EOR abs,X", ABSX, eor},
    [0x5e] = {"LSR abs,X", ABSX, lsr},
    [0x5f] = {"SRE abs,X", ABSX, sre},
    [0x60] = {"RTS impl",  IMPL, rts},
    [0x61] = {"ADC X,ind", XIND, adc},
    [0x62] = {"JAM",       NONE, jam},
    [0x63] = {"RRA X,ind", XIND, rra},
    [0x64] = {"NOP zpg",   ZP,   nop},
    [0x65] = {"ADC zpg",   ZP,   adc},
    [0x66] = {"ROR zpg",   ZP,   ror},
    [0x67] = {"RRA zpg",   ZP,   rra},
    [0x68] = {"PLA impl",  IMPL, pla},
    [0x69] = {"ADC #",     IMM,  adc},
    [0x6a] = {"ROR A",     ACC,  ror},
    [0x6b] = {"ARR #",     IMM,  arr},
    [0x6c] = {"JMP ind",   IND,  jmp},
    [0x6d] = {"ADC abs",   ABS,  adc},
    [0x6e] = {"ROR abs",   ABS,  ror},
    [0x6f] = {"RRA abs",   ABS,  rra},
    [0x70] = {"BVS rel",   REL,  bvs},
    [0x71] = {"ADC ind,Y", INDY, adc},
    [0x72] = {"JAM",       NONE, jam},
    [0x73] = {"RRA ind,Y", INDY, rra},
    [0x74] = {"NOP zpg,X", ZPX,  nop},
    [0x75] = {"ADC zpg,X", ZPX,  adc},
    [0x76] = {"ROR zpg,X", ZPX,  ror},
    [0x77] = {"RRA zpg,X", ZPX,  rra},
    [0x78] = {"SEI impl",  IMPL, sei},
    [0x79] = {"ADC abs,Y", ABSY, adc},
    [0x7a] = {"NOP impl",  IMPL, nop},
    [0x7b] = {"RRA abs,Y", ABSY, rra},
    [0x7c] = {"NOP abs,X", ABSX, nop},
    [0x7d] = {"ADC abs,X", ABSX, adc},
    [0x7e] = {"ROR abs,X", ABSX, ror},
    [0x7f] = {"RRA abs,X", ABSX, rra},
    [0x80] = {"NOP #",     IMM,  nop},
    [0x81] = {"STA X,ind", XIND, sta},
    [0x82] = {"NOP #",     IMM,  nop},
    [0x83] = {"SAX X,ind", XIND, sax},
    [0x84] = {"STY zpg",   ZP,   sty},
    [0x85] = {"STA zpg",   ZP,   sta},
    [0x86] = {"STX zpg",   ZP,   stx},
    [0x87] = {"SAX zpg",   ZP,   sax},
    [0x88] = {"DEY impl",  IMPL, dey},
    [0x89] = {"NOP #",     IMM,  nop},
    [0x8a] = {"TXA impl",  IMPL, txa},
    [0x8b] = {"ANE #",     IMM,  ane},
    [0x8c] = {"STY abs",   ABS,  sty},
    [0x8d] = {"STA abs",   ABS,  sta},
    [0x8e] = {"STX abs",   ABS,  stx},
    [0x8f] = {"SAX abs",   ABS,  sax},
    [0x90] = {"BCC rel",   REL,  bcc},
    [0x91] = {"STA ind,Y", INDY, sta},
    [0x92] = {"JAM",       NONE, jam},
    [0x93] = {"SHA ind,Y", INDY, sha},
    [0x94] = {"STY zpg,X", ZPX,  sty},
    [0x95] = {"STA zpg,X", ZPX,  sta},
    [0x96] = {"STX zpg,Y", ZPY,  stx},
    [0x97] = {"SAX zpg,Y", ZPY,  sax},
    [0x98] = {"TYA impl",  IMPL, tya},
    [0x99] = {"STA abs,Y", ABSY, sta},
    [0x9a] = {"TXS impl",  IMPL, txs},
    [0x9b] = {"TAS abs,Y", ABSY, tas},
    [0x9c] = {"SHY abs,X", ABSX, shy},
    [0x9d] = {"STA abs,X", ABSX, sta},
    [0x9e] = {"SHX abs,Y", ABSY, shx},
    [0x9f] = {"SHA abs,Y", ABSY, sha},
    [0xa0] = {"LDY #",     IMM,  ldy},
    [0xa1] = {"LDA X,ind", XIND, lda},
    [0xa2] = {"LDX #",     IMM,  ldx},
    [0xa3] = {"LAX X,ind", XIND, lax},
    [0xa4] = {"LDY zpg",   ZP,   ldy},
    [0xa5] = {"LDA zpg",   ZP,   lda},
    [0xa6] = {"LDX zpg",   ZP,   ldx},
    [0xa7] = {"LAX zpg",   ZP,   lax},
    [0xa8] = {"TAY impl",  IMPL, tay},
    [0xa9] = {"LDA #",     IMM,  lda},
    [0xaa] = {"TAX impl",  IMPL, tax},
    [0xab] = {"LXA #",     IMM,  lxa},
    [0xac] = {"LDY abs",   ABS,  ldy},
    [0xad] = {"LDA abs",   ABS,  lda},
    [0xae] = {"LDX abs",   ABS,  ldx},
    [0xaf] = {"LAX abs",   ABS,  lax},
    [0xb0] = {"BCS rel",   REL,  bcs},
    [0xb1] = {"LDA ind,Y", INDY, lda},
    [0xb2] = {"JAM",       NONE, jam},
    [0xb3] = {"LAX ind,Y", INDY, lax},
    [0xb4] = {"LDY zpg,X", ZPX,  ldy},
    [0xb5] = {"LDA zpg,X", ZPX,  lda},
    [0xb6] = {"LDX zpg,Y", ZPY,  ldx},
    [0xb7] = {"LAX zpg,Y", ZPY,  lax},
    [0xb8] = {"CLV impl",  IMPL, clv},
    [0xb9] = {"LDA abs,Y", ABSY, lda},
    [0xba] = {"TSX impl",  IMPL, tsx},
    [0xbb] = {"LAS abs,Y", ABSY, las},
    [0xbc] = {"LDY abs,X", ABSX, ldy},
    [0xbd] = {"LDA abs,X", ABSX, lda},
    [0xbe] = {"LDX abs,Y", ABSY, ldx},
    [0xbf] = {"LAX abs,Y", ABSY, lax},
    [0xc0] = {"CPY #",     IMM,  cpy},
    [0xc1] = {"CMP X,ind", XIND, cmp},
    [0xc2] = {"NOP #",     IMM,  nop},
    [0xc3] = {"DCP X,ind", XIND, dcp},
    [0xc4] = {"CPY zpg",   ZP,   cpy},
    [0xc5] = {"CMP zpg",   ZP,   cmp},
    [0xc6] = {"DEC zpg",   ZP,   dec},
    [0xc7] = {"DCP zpg",   ZP,   dcp},
    [0xc8] = {"INY impl",  IMPL, iny},
    [0xc9] = {"CMP #",     IMM,  cmp},
    [0xca] = {"DEX impl",  IMPL, dex},
    [0xcb] = {"SBX #",     IMM,  sbx},
    [0xcc] = {"CPY abs",   ABS,  cpy},
    [0xcd] = {"CMP abs",   ABS,  cmp},
    [0xce] = {"DEC abs",   ABS,  dec},
    [0xcf] = {"DCP abs",   ABS,  dcp},
    [0xd0] = {"BNE rel",   REL,  bne},
    [0xd1] = {"CMP ind,Y", INDY, cmp},
    [0xd2] = {"JAM",       NONE, jam},
    [0xd3] = {"DCP ind,Y", INDY, dcp},
    [0xd4] = {"NOP zpg,X", ZPX,  nop},
    [0xd5] = {"CMP zpg,X", ZPX,  cmp},
    [0xd6] = {"DEC zpg,X", ZPX,  dec},
    [0xd7] = {"DCP zpg,X", ZPX,  dcp},
    [0xd8] = {"CLD impl",  IMPL, cld},
    [0xd9] = {"CMP abs,Y", ABSY, cmp},
    [0xda] = {"NOP impl",  IMPL, nop},
    [0xdb] = {"DCP abs,Y", ABSY, dcp},
    [0xdc] = {"NOP abs,X", ABSX, nop},
    [0xdd] = {"CMP abs,X", ABSX, cmp},
    [0xde] = {"DEC abs,X", ABSX, dec},
    [0xdf] = {"DCP abs,X", ABSX, dcp},
    [0xe0] = {"CPX #",     IMM,  cpx},
    [0xe1] = {"SBC X,ind", XIND, sbc},
    [0xe2] = {"NOP #",     IMM,  nop},
    [0xe3] = {"ISC X,ind", XIND, isc},
    [0xe4] = {"CPX zpg",   ZP,   cpx},
    [0xe5] = {"SBC zpg",   ZP,   sbc},
    [0xe6] = {"INC zpg",   ZP,   inc},
    [0xe7] = {"ISC zpg",   ZP,   isc},
    [0xe8] = {"INX impl",  IMPL, inx},
    [0xe9] = {"SBC #",     IMM,  sbc},
    [0xea] = {"NOP impl",  IMPL, nop},
    [0xeb] = {"USBC #",    IMM,  usbc},
    [0xec] = {"CPX abs",   ABS,  cpx},
    [0xed] = {"SBC abs",   ABS,  sbc},
    [0xee] = {"INC abs",   ABS,  inc},
    [0xef] = {"ISC abs",   ABS,  isc},
    [0xf0] = {"BEQ rel",   REL,  beq},
    [0xf1] = {"SBC ind,Y", INDY, sbc},
    [0xf2] = {"JAM",       NONE,  jam},
    [0xf3] = {"ISC ind,Y", INDY, isc},
    [0xf4] = {"NOP zpg,X", ZPX,   nop},
    [0xf5] = {"SBC zpg,X", ZPX,   sbc},
    [0xf6] = {"INC zpg,X", ZPX,   inc},
    [0xf7] = {"ISC zpg,X", ZPX,   isc},
    [0xf8] = {"SED impl",  IMPL, sed},
    [0xf9] = {"SBC abs,Y", ABSY,  sbc},
    [0xfa] = {"NOP impl",  IMPL, nop},
    [0xfb] = {"ISC abs,Y", ABSY,  isc},
    [0xfc] = {"NOP abs,X", ABSX,  nop},
    [0xfd] = {"SBC abs,X", ABSX,  sbc},
    [0xfe] = {"INC abs,X", ABSX,  inc},
    [0xff] = {"ISC abs,X", ABSX,  isc},
};

/* other utils */
void cpu_get_opcode_info(char *ret, uint8_t opcode)
{
    strcpy(ret, opcode_table[opcode].name);
}

static void handle_addressing_mode(struct nes *nes, addr_mode_t addr_mode)
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
        // the RTS instruction only read operand 1 before pushing PC 
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
        operand_8 = fetch_8(nes);
        // cycle #3
        cpu_read(nes, operand_8);
        nes->cpu.effective_addr = (operand_8 + nes->cpu.x) & 0x00ff;
        break;
    case ZPY:
        // cycle #2
        operand_8 = fetch_8(nes);
        // cycle #3
        cpu_read(nes, operand_8);
        nes->cpu.effective_addr = (operand_8 + nes->cpu.y) & 0x00ff;
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
        operand_8 = fetch_8(nes);
        // cycle #3
        cpu_read(nes, operand_8);
        // cycle #4
        lb = cpu_read(nes, (operand_8 + nes->cpu.x) & 0x00ff);
        // cycle #5
        hb = cpu_read(nes, (operand_8 + nes->cpu.x + 1) & 0x00ff);
        nes->cpu.effective_addr = TO_U16(lb, hb);
        break;
    case INDY:
        // cycle #2
        operand_8 = fetch_8(nes);
        // cycle #3
        lb = cpu_read(nes, operand_8);
        // cycle #4
        hb = cpu_read(nes, (operand_8 + 1) & 0x00ff);
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
    write_log(nes);
    nes->cpu.opcode = fetch_8(nes);
    handle_addressing_mode(nes, opcode_table[nes->cpu.opcode].addr_mode);
    // run the opcode execution
    opcode_table[nes->cpu.opcode].handler(nes, opcode_table[nes->cpu.opcode].addr_mode);

    // interrupt polling 
    interrupt_process(nes);
}

void cpu_at_power_up(struct nes *nes)
{
    nes->cpu.p = 0x24;
    nes->cpu.a = 0;
    nes->cpu.x = 0;
    nes->cpu.y = 0;
    nes->cpu.sp = 0xfd;

    nes->mem[0x4017] = 0x00;
    nes->mem[0x4015] = 0x00;
    memset(&nes->mem[0x4000], 0, 0x10);
    memset(&nes->mem[0x4010], 0, 0x04);
    // TODO: APU registers state
    nes->cpu.interrupt_pending = 0;
}