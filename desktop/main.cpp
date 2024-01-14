#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "nes.h"
#include "cpu.h"
#include "cart.h"
#include <SDL2/SDL.h>

int main(int argc, char*argv[])
{
    struct nes nes;

    if (argc == 1) {
        printf("Need to supply a ROM file\n");
        exit(EXIT_FAILURE);
    }
    cpu_at_power_up(&nes);
    cart_load(&nes, argv[1]);
    cart_print_info(&nes.cart.info);
    nes.cpu.pc = 0xc000;
    while (1) {
        cpu_step(&nes);
    }
    return 0;
}