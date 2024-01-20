#pragma once

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL2/SDL.h>
#include "nes.h"
#include "cpu.h"
#include "utils.h"

#define PATTERN_TABLE_WIDTH     256
#define PATTERN_TABLE_HEIGHT    128

// macros
#define COL(value) (value / 255)

struct gui {
    bool show_cpu_debug_window;

    /* SDL - related */
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *screen_texture;
    SDL_Texture *debug_texture;

    uint32_t *screen_buffer;
    uint32_t *debug_buffer;

    /* disassembler */
    char instr_table[14][20];
};

void sdl_setup(struct gui *gui);
void gui_setup(struct gui *gui);
void gui_destroy(void);
void sdl_destroy(struct gui *gui);
void render(struct gui *gui, struct nes *nes);
