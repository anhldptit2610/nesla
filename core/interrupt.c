#include "interrupt.h"

uint16_t vector_base[] = {
    [NMI] = NMI_VECTOR_BASE,
    [IRQ] = IRQ_BRK_VECTOR_BASE,
};

void interrupt_handler(struct nes *nes, interrupt_t interrupt)
{
    uint8_t pcl, pch;

    cpu_read(nes, nes->cpu.pc);
    cpu_read(nes, nes->cpu.pc);
    stack_push_16(nes, nes->cpu.pc);
    stack_push_8(nes, nes->cpu.p & ~(1U << 4));
    nes->cpu.I = 1;
    pcl = cpu_read(nes, vector_base[interrupt]);
    pch = cpu_read(nes, vector_base[interrupt] + 1);
    nes->cpu.pc = TO_U16(pcl, pch);
}

void interrupt_process(struct nes *nes)
{
    if (nes->cpu.interrupt_pending & NMI) {
        interrupt_handler(nes, NMI);
        // if both NMI and IRQ interrupt are pending, run NMI handler and forget 
        // the IRQ pending.
        nes->cpu.interrupt_pending &= ~IRQ;
        nes->cpu.interrupt_pending &= ~NMI;
    }
    if ((nes->cpu.interrupt_pending & IRQ) && !nes->cpu.I) {
        interrupt_handler(nes, IRQ);
        nes->cpu.interrupt_pending &= ~IRQ;
    }
}