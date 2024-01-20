#pragma once

#include "nes.h"
#include "cpu.h"
#include "render.h"

void disassemble(struct nes *nes, char instr_str[13][20]);
void parse_pattern_table_entry(struct nes *nes, uint32_t *buffer, int x, int y);