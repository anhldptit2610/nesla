#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "nes.h"
#include "cpu.h"
#include "cart.h"
#include "render.h"
#include "utils.h"
#include <SDL2/SDL.h>

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry()
#endif

int main(int argc, char*argv[])
{
    struct gui gui;
    struct nes nes;

    sdl_setup(&gui);
    gui_setup(&gui);

    // setup NES system
    cpu_at_power_up(&nes);
    ppu_at_power_up(&nes);
    cart_load(&nes, argv[1]);
    cart_print_info(&nes.cart.info);
    nes.cache_size = 0;
    nes.step = false;
    nes.cpu.pc = 0xc000;
    nes.run_mode = NORMAL;

    // main loop
    bool done = false;
    bool show_demo_window = true;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && 
                event.window.windowID == SDL_GetWindowID(gui.window))
                done = true;
        }
        if (nes.step && nes.run_mode == STEP) {
            cpu_step(&nes);
            nes.step = false;
        } else if (nes.run_mode == NORMAL) {
            cpu_step(&nes);
        }
 
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 32; x++) {
                parse_pattern_table_entry(&nes, gui.debug_buffer, x, y);
            }
        }
        SDL_UpdateTexture(gui.debug_texture, NULL, gui.debug_buffer, PATTERN_TABLE_WIDTH * 4);

        disassemble(&nes, gui.instr_table);
        render(&gui, &nes);
    }

    gui_destroy();
    sdl_destroy(&gui);
    return 0;
}