#include "interrupt.h"

uint16_t vector_base[] = {
    [NMI] = NMI_VECTOR_BASE,
    [IRQ] = IRQ_BRK_VECTOR_BASE,
};

void interrupt_handler(struct nes *nes, interrupt_t interrupt)
{
    uint8_t pcl, pch;
    uint16_t base_addr;

    cpu_read(nes, nes->cpu.pc);
    cpu_read(nes, nes->cpu.pc);
    stack_push_16(nes, nes->cpu.pc);
    if (interrupt == IRQ && nes->cpu.nmi_pending) {
        base_addr = NMI_VECTOR_BASE;
        nes->cpu.nmi_pending = 0;
    } else {
        base_addr = IRQ_BRK_VECTOR_BASE;
        nes->cpu.irq_pending = 0;
    }
    stack_push_8(nes, nes->cpu.p & ~(1U << 4));
    nes->cpu.I = 1;
    pcl = cpu_read(nes, base_addr);
    pch = cpu_read(nes, base_addr + 1);
    nes->cpu.pc = TO_U16(pcl, pch);
}

void interrupt_process(struct nes *nes)
{
    if (nes->cpu.prev_nmi_pending) {
        interrupt_handler(nes, NMI);
        // if both NMI and IRQ interrupt are pending, run NMI handler and forget 
        // the IRQ pending.
        nes->cpu.nmi_pending = 0;
    } else if (nes->cpu.prev_irq_pending && !nes->cpu.I) {
        interrupt_handler(nes, IRQ);
    }
}