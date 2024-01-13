#include "common.h"
#include "nes.h"
#include "cpu.h"

int main(int argc, char *argv[])
{
    struct nes nes;

    cpu_at_power_up(&nes);
    return 0;
}