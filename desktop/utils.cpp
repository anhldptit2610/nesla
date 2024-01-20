#include "utils.h"

uint32_t default_color[] = {0x9bbc0fff, 0x8bac0fff, 0x306230ff, 0x0f380fff};

void create_instr_string(struct nes *nes, char *str, uint16_t pc)
{
    char opcode_name[5];

    nes->cpu.pc = pc;
    nes->cpu.opcode = cpu_read(nes, nes->cpu.pc++);
    get_opcode_name(nes->cpu.opcode, opcode_name);
    handle_addressing_mode(nes, get_opcode_mode(nes->cpu.opcode));
    switch (get_opcode_mode(nes->cpu.opcode)) {
    case ACC:
        sprintf(str, "$%04x: %s A", pc, opcode_name);
        break;
    case IMPL:
        sprintf(str, "$%04x: %s", pc, opcode_name);
        break;
    case IMM:
        sprintf(str, "$%04x: %s #$%02x", pc, opcode_name, nes->cpu.operation_value);    
        break;
    case ABS:
        if (nes->cpu.opcode == 0x20)
            nes->cpu.effective_addr = TO_U16(nes->cpu.operand[0], cpu_read(nes, nes->cpu.pc++));
        sprintf(str, "$%04x: %s $%04x", pc, opcode_name, nes->cpu.effective_addr);
        break;
    case ZP:
        sprintf(str, "$%04x: %s $%02x", pc, opcode_name, nes->cpu.effective_addr);
        break;
    case ABSX:
        sprintf(str, "$%04x: %s $%04x,X", pc, opcode_name, nes->cpu.effective_addr);
        break;
    case ABSY:
        sprintf(str, "$%04x: %s $%04x,Y", pc, opcode_name, nes->cpu.effective_addr);
        break;
    case ZPX:
        sprintf(str, "$%04x: %s $%02x,X", pc, opcode_name, nes->cpu.operand[0]);    
        break;
    case ZPY:
        sprintf(str, "$%04x: %s $%02x,Y", pc, opcode_name, nes->cpu.operand[0]);    
        break;
    case IND:
        sprintf(str, "$%04x: %s ($%04x)", pc, opcode_name, nes->cpu.effective_addr);
        break;
    case XIND:
        sprintf(str, "$%04x: %s ($%02x,X)", pc, opcode_name, nes->cpu.operand[0]);
        break;
    case INDY:
        sprintf(str, "$%04x: %s ($%02x),Y", pc, opcode_name, nes->cpu.operand[0]);
        break;
    case REL:
        sprintf(str, "$%04x: %s $%04x", pc, opcode_name, nes->cpu.effective_addr);
        break;
    default:
        break;
    }
}

void disassemble(struct nes *nes, char instr_str[13][20])
{
    uint16_t start_pc = nes->cpu.pc;
    uint16_t instr_addr;
    int i;

    for (i = 0; i <= 5; i++) {
        if (!(instr_addr = cache_get_nth_element(nes, i + 1))) 
            break;
        uint16_t current_pc = nes->cpu.pc; 
        create_instr_string(nes, instr_str[nes->cache_size - 1 - i], instr_addr); 
        nes->cpu.pc = current_pc;
    }

    for (i; i < 13; i++)
        create_instr_string(nes, instr_str[i], nes->cpu.pc);
    nes->cpu.pc = start_pc;
}

void parse_pattern_table_entry(struct nes *nes, uint32_t *buffer, int x, int y)
{
    uint32_t a, b, offset;

    offset = (x > 15) ? 4096 - 16 * 16 : 0;
    for (int i = 0; i < 8; i++) {
        uint8_t plane_0 = nes->cart.chr_rom[x * 16 + y * 256 + offset + i];
        uint8_t plane_1 = nes->cart.chr_rom[x * 16 + y * 256 + offset + i + 8];
        for (int j = 0; j < 8; j++) {
            uint8_t lb = (plane_1 >> (7 - j)) & 0x01;
            uint8_t hb = (plane_0 >> (7 - j)) & 0x01;
            buffer[j + i * PATTERN_TABLE_WIDTH + x * 8 + y * 8 * PATTERN_TABLE_WIDTH] = default_color[(hb << 1) | lb];
        }
    }
}