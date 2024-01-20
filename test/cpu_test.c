#include <cjson/cJSON.h>
#include "nes.h"
#include "cpu.h"

struct cpu_state {
    uint8_t opcode;
    uint16_t pc;
    uint8_t sp;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t p;

    struct memory {
        uint16_t addr;
        uint8_t val;
    } mem[100];
    int mem_index;

#ifdef CYCLE_DEBUG
    struct memory_access_record record[10];
    int record_index;
#endif
};

void print_test_info(struct cpu_state initial, struct cpu_state final)
{
    printf("Initial:\n");
    printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
                initial.pc, initial.sp, initial.a, initial.x, initial.y, initial.p);
    printf("mem: ");
    for (int i = 0; i < initial.mem_index; i++)
        printf("%04x - %02x ", initial.mem[i].addr, initial.mem[i].val);
    printf("\nFinal:\n");
    printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
                final.pc, final.sp, final.a, final.x, final.y, final.p);
    printf("mem: ");
    for (int i = 0; i < final.mem_index; i++)
        printf("%04x - %02x ", final.mem[i].addr, final.mem[i].val);
#ifdef CYCLE_DEBUG
    printf("\n\nCycles:\n");
    for (int i = 0; i < final.record_index; i++)
        printf("%04x - %02x - %s\n", final.record[i].addr, final.record[i].val,
                (final.record[i].action == R) ? "READ" : "W");
#endif
}

void read_json(char buffer[1024], FILE *fp)
{
    int j = 0, times = 0, c;

    while (times < 3) {
        if ((c = getc(fp)) == '}')
            times++;
        buffer[j++] = c;
    }
    buffer[j] = '\0';
    fgetc(fp);
    fgetc(fp);
}

void setup_test(char *test, struct nes *nes, char *test_name, 
                struct cpu_state *initial_state, struct cpu_state *final_state)
{
    cJSON *name = NULL, *initial = NULL, *final = NULL, *cycles = NULL;
    cJSON *json_test = cJSON_Parse(test);
    cJSON *node = NULL, *element = NULL, *j = NULL;
    uint16_t state_buffer[6], mem_buffer[2], i, k;

    if (!test) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
            printf("Error: %s\n", error_ptr);
        goto parse_failed;
    }

    // parse the test name
    name = cJSON_GetObjectItemCaseSensitive(json_test, "name");
    strcpy(test_name, name->valuestring);

    // parse initial state 
    initial = cJSON_GetObjectItemCaseSensitive(json_test, "initial");
    if (!initial) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
            printf("Error: %s\n", error_ptr);
        goto parse_failed;
    }

    node = initial->child;
    i = 0;
    while (node->next) {
        state_buffer[i++] = node->valueint;
        node = node->next;
    }
    cJSON_ArrayForEach(element, node)
    {
        k = 0;
        cJSON_ArrayForEach(j, element)
        {
            mem_buffer[k++] = j->valueint;
        }
        initial_state->mem[initial_state->mem_index].addr = mem_buffer[0];
        initial_state->mem[initial_state->mem_index].val = mem_buffer[1];
        initial_state->mem_index++;
        nes->cpu.mem[mem_buffer[0]] = mem_buffer[1];
    }
    initial_state->pc = nes->cpu.pc = state_buffer[0];
    initial_state->sp = nes->cpu.sp = state_buffer[1];
    initial_state->a = nes->cpu.a = state_buffer[2];
    initial_state->x = nes->cpu.x = state_buffer[3];
    initial_state->y = nes->cpu.y = state_buffer[4];
    initial_state->p = nes->cpu.p = state_buffer[5];
    initial_state->opcode = nes->cpu.mem[initial_state->pc];

    // parse final state
    final = cJSON_GetObjectItemCaseSensitive(json_test, "final");
    if (!final) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
            printf("Error: %s\n", error_ptr);
        goto parse_failed;
    }

    node = final->child;
    i = 0;
    while (node->next) {
        state_buffer[i++] = node->valueint;
        node = node->next;
    }
    cJSON_ArrayForEach(element, node)
    {
        k = 0;
        cJSON_ArrayForEach(j, element)
        {
            mem_buffer[k++] = j->valueint;
        }
        final_state->mem[final_state->mem_index].addr = mem_buffer[0];
        final_state->mem[final_state->mem_index].val = mem_buffer[1];
        final_state->mem_index++;
    }
    final_state->pc = state_buffer[0];
    final_state->sp = state_buffer[1];
    final_state->a = state_buffer[2];
    final_state->x = state_buffer[3];
    final_state->y = state_buffer[4];
    final_state->p = state_buffer[5];

#ifdef CYCLE_DEBUG
    // parse cycles
    cycles = cJSON_GetObjectItemCaseSensitive(json_test, "cycles");
    cJSON_ArrayForEach(element, cycles)
    {
        k = 0;
        cJSON_ArrayForEach(j, element)
        {
            if (cJSON_IsString(j)) {
                if (!strcmp(j->valuestring, "read")) 
                    final_state->record[final_state->record_index].action = R;
                else if (!strcmp(j->valuestring, "write"))
                    final_state->record[final_state->record_index].action = W;
            } else {
                mem_buffer[k++] = j->valueint;
            }
        }
        final_state->record[final_state->record_index].addr = mem_buffer[0];
        final_state->record[final_state->record_index].val = mem_buffer[1];
        final_state->record_index++;
    }
#endif

parse_failed:
    cJSON_Delete(json_test);
}

int check_reg_and_mem(struct nes *nes, struct cpu_state *final)
{
    int ret = 0;
    bool register_failed = false, memory_failed = false;

    // check register state
    if ((nes->cpu.pc != final->pc) || (nes->cpu.a != final->a) || (nes->cpu.x != final->x) ||
        (nes->cpu.y != final->y) || (nes->cpu.p != final->p) || (nes->cpu.sp != final->sp))
        register_failed = true;

    // check memory state
    for (int i = 0; i < final->mem_index; i++) {
        uint16_t addr = final->mem[i].addr;
        if (nes->cpu.mem[addr] != final->mem[i].val) {
            memory_failed = true;
            break;
        }
    }
    if (register_failed && !memory_failed)
        ret = 1;
    else if (!register_failed && memory_failed)
        ret = 2;
    else if (register_failed && memory_failed)
        ret = 3;
    return ret;
}

#ifdef CYCLE_DEBUG
int check_cycles(struct nes *nes, struct cpu_state *final)
{
    int ret = 0;

    for (int i = 0; i < final->record_index; i++) {
        if (nes->cpu.record[i].addr != final->record[i].addr ||
            nes->cpu.record[i].val != final->record[i].val ||
            nes->cpu.record[i].action != final->record[i].action) {
            ret = 1;
            break;
        }
    }
    return ret;
}
#endif

void print_reg_and_mem_error(char *test_name, char *opcode_info, struct nes *nes,
                            struct cpu_state *initial, struct cpu_state *final, int ret)
{
    printf("Register and mem test failed.\n Case: %s. Opcode name & mode: %s\n", test_name, opcode_info);
    if (ret == 1)
        printf("Reason: register state not matched\n");
    else if (ret == 2)
        printf("Reason: memory not matched\n");
    else if (ret == 3)
        printf("Reason: register state and memory not matched\n");
    printf("--------------------------------------------\n");
    printf("Initial state:\n");
    printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
            initial->pc, initial->sp, initial->a, initial->x, initial->y, initial->p);
    printf("mem: ");
    for (int i = 0; i < initial->mem_index; i++)
        printf("%04x - %02x ", initial->mem[i].addr, initial->mem[i].val);
    printf("\n--------------------------------------------\n");
    printf("CPU state:\n");
    printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
            nes->cpu.pc, nes->cpu.sp, nes->cpu.a, nes->cpu.x, nes->cpu.y, nes->cpu.p);
    printf("mem: ");
    for (int i = 0; i < final->mem_index; i++)
        printf("%04x - %02x ", final->mem[i].addr, nes->cpu.mem[final->mem[i].addr]);
    printf("\n--------------------------------------------\n");
    printf("final state:\n");
    printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
            final->pc, final->sp, final->a, final->x, final->y, final->p);
    printf("mem: ");
    for (int i = 0; i < final->mem_index; i++)
        printf("%04x - %02x ", final->mem[i].addr, final->mem[i].val);
    printf("\n");
}

#ifdef CYCLE_DEBUG
void print_cycle_error(struct nes *nes, struct cpu_state *final,
                        char *test_name, char *opcode_info)
{
    printf("Cycle check failed.\nCase: %s Opcode_info: %s\nComparision:\n",
            test_name, opcode_info);
    printf("NES----------------------test\n");
    for (int i = 0; i < final->record_index; i++) {
        printf("%04x - %02x - %s | %04x - %02x - %s\n",
                nes->cpu.record[i].addr, nes->cpu.record[i].val,
                (nes->cpu.record[i].action == R) ? "READ " : "WRITE",
                final->record[i].addr, final->record[i].val,
                (final->record[i].action == R) ? "READ " : "WRITE");
    }
}
#endif

int main(int argc, char *argv[])
{
    struct cpu_state initial, final;
    int ret;

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Can't open file %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // read the redundant characters in file
    fgetc(fp);


    for (int m = 0; m < 10000; m++) {
        char json_test[2048], test_name[50], opcode_name[20];
        struct nes nes;

        initial.mem_index = final.mem_index = 0;
#ifdef CYCLE_DEBUG
        final.record_index = 0;
#endif
        read_json(json_test, fp);
        setup_test(json_test, &nes, test_name, &initial, &final);
        cpu_get_opcode_info(opcode_name, initial.opcode);
        cpu_step(&nes);

        if ((ret = check_reg_and_mem(&nes, &final)) > 0) {
            print_reg_and_mem_error(test_name, opcode_name, &nes, &initial, &final, ret);
            goto error;
        } 
#ifdef CYCLE_DEBUG
        if ((ret = check_cycles(&nes, &final)) > 0) {
            print_cycle_error(&nes, &final, test_name, opcode_name);
            goto error;
        }
#endif
    }
    printf("Test %s ok\n", argv[1]);
    printf("*******************************************************************\n");
    return 0;

error:
    exit(EXIT_FAILURE);
}