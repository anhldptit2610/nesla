#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "nes.h"
#include "cpu.h"

#define NMI_VECTOR_BASE         0xfffa
#define RESET_VECTOR_BASE       0xfffc
#define IRQ_BRK_VECTOR_BASE     0xfffe

void interrupt_handler(struct nes *nes, interrupt_t interrupt);
void interrupt_process(struct nes *nes);

#ifdef __cplusplus
}
#endif