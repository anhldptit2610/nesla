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
};

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
    cJSON *name = NULL, *initial = NULL, *final = NULL;
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
        nes->mem[mem_buffer[0]] = mem_buffer[1];
    }
    initial_state->pc = nes->cpu.pc = state_buffer[0];
    initial_state->sp = nes->cpu.sp = state_buffer[1];
    initial_state->a = nes->cpu.a = state_buffer[2];
    initial_state->x = nes->cpu.x = state_buffer[3];
    initial_state->y = nes->cpu.y = state_buffer[4];
    initial_state->p = nes->cpu.p = state_buffer[5];
    initial_state->opcode = nes->mem[initial_state->pc];
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

parse_failed:
    cJSON_Delete(json_test);
}

int check_test(struct nes *nes, struct cpu_state *final)
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
        if (nes->mem[addr] != final->mem[i].val) {
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
        initial.mem_index = final.mem_index = 0;
        char json_test[2048], test_name[50], opcode_name[20];
        read_json(json_test, fp);
        struct nes nes;
        setup_test(json_test, &nes, test_name, &initial, &final);
        cpu_get_opcode_info(opcode_name, initial.opcode);
        cpu_step(&nes);
        if ((ret = check_test(&nes, &final)) > 0) {
            printf("Test failed at case %s. Opcode name & mode: %s\n", test_name, opcode_name);
            if (ret == 1)
                printf("Reason: register state not matched\n");
            else if (ret == 2)
                printf("Reason: memory not matched\n");
            else if (ret == 3)
                printf("Reason: register state and memory not matched\n");
            printf("--------------------------------------------\n");
            printf("Initial state:\n");
            printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
                    initial.pc, initial.sp, initial.a, initial.x, initial.y, initial.p);
            printf("mem: ");
            for (int i = 0; i < initial.mem_index; i++)
                printf("%04x - %02x ", initial.mem[i].addr, initial.mem[i].val);
            printf("\n--------------------------------------------\n");
            printf("CPU state:\n");
            printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
                    nes.cpu.pc, nes.cpu.sp, nes.cpu.a, nes.cpu.x, nes.cpu.y, nes.cpu.p);
            printf("mem: ");
            for (int i = 0; i < final.mem_index; i++)
                printf("%04x - %02x ", final.mem[i].addr, nes.mem[final.mem[i].addr]);
            printf("\n--------------------------------------------\n");
            printf("final state:\n");
            printf("pc - %04x sp - %02x a - %02x x - %02x y - %02x p - %02x\n",
                    final.pc, final.sp, final.a, final.x, final.y, final.p);
            printf("mem: ");
            for (int i = 0; i < final.mem_index; i++)
                printf("%04x - %02x ", final.mem[i].addr, final.mem[i].val);
            printf("\n");
            exit(EXIT_FAILURE);
        } 
    }
    printf("Test %s ok\n", argv[1]);
    printf("*******************************************************************\n");
    return 0;
}