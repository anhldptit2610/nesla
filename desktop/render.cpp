#include "render.h"

ImVec4 ImU32toIV4(ImU32 color) {
	return ImGui::ColorConvertU32ToFloat4(color);
}

#define COLOR_WHITE             ImU32toIV4(IM_COL32(255, 255, 255, 255))
#define COLOR_LIGHTGREEN        ImU32toIV4(IM_COL32(155, 188, 15, 255))
#define COLOR_DARKGREEN         ImU32toIV4(IM_COL32(48, 98, 48, 255))

void sdl_setup(struct gui *gui)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER)) {
        SDL_Log("Error - SDL_Init: %s\n", SDL_GetError());
        return;
    }

    // from 2.0.18: Enable native IME
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    gui->window = SDL_CreateWindow("nesla", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!gui->window) {
        SDL_Log("Error - SDL_CreateWindow: %s\n", SDL_GetError());
        return;
    }

    gui->renderer = SDL_CreateRenderer(gui->window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!gui->renderer) {
        SDL_Log("Error - SDL_CreateRenderer: %s\n", SDL_GetError());
        return;
    }

    gui->screen_texture = SDL_CreateTexture(gui->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                            SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!gui->screen_texture) {
        SDL_Log("Error - SDL_CreateTexture: %s\n", SDL_GetError());
        return;
    }
    gui->screen_buffer = (uint32_t *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    if (!gui->screen_buffer) {
        printf("Malloc error\n");
        return;
    }

    gui->debug_texture = SDL_CreateTexture(gui->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                            PATTERN_TABLE_WIDTH, PATTERN_TABLE_HEIGHT);
    if (!gui->debug_texture) {
        SDL_Log("Error - SDL_CreateTexture: %s\n", SDL_GetError());
        return;
    }
    gui->debug_buffer = (uint32_t *)malloc(PATTERN_TABLE_WIDTH * PATTERN_TABLE_HEIGHT * 4);
    if (!gui->debug_buffer) {
        printf("Malloc error\n");
        return;
    }
}

void gui_setup(struct gui *gui)
{
    // setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle *style = &ImGui::GetStyle();
    style->Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(IM_COL32(48, 98, 48, 255));
    style->Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(155, 188, 15, 255));
    style->Colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(155, 188, 15, 255));
    style->Colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(48, 98, 48, 255));

    // setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(gui->window, gui->renderer);
    ImGui_ImplSDLRenderer2_Init(gui->renderer);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
}

void gui_destroy(void)
{
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void sdl_destroy(struct gui *gui)
{
    SDL_DestroyRenderer(gui->renderer);
    SDL_DestroyWindow(gui->window);
    SDL_Quit();
}

void render(struct gui *gui, struct nes *nes)
{
    static int pause = 0;
    int i;
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    char instr_str[14][20];

    // start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    // PPU pattern table    
    ImGui::Begin("Pattern table");
    ImGui::Image((ImTextureID)gui->debug_texture, ImVec2(512, 256));
    ImGui::End();

    // PPU screen
    ImGui::Begin("Screen");
    ImGui::Image((ImTextureID)gui->screen_texture, ImVec2(512, 480));
    ImGui::End();

    ImGui::Begin("CPU debug");
    // CPU debug window
    if (ImGui::Button("normal")) {
        nes->run_mode = NORMAL;
    }
    ImGui::SameLine();
    if (ImGui::Button("step")) {
        nes->step = true; 
        nes->run_mode = STEP;
    }
    ImGui::SameLine();
    if (ImGui::Button("pause")) {
        if (nes->run_mode == NORMAL)
            nes->run_mode = PAUSE;
        else if (nes->run_mode == PAUSE)
            nes->run_mode = NORMAL;
    }

    ImGui::SeparatorText("registers");
    ImGui::Text("PC: %04x A: %02x X:%02x Y:%02x P:%02x SP:%02x",
                nes->cpu.pc, nes->cpu.a, nes->cpu.x, nes->cpu.y, nes->cpu.p, nes->cpu.sp);

    ImGui::SeparatorText("flags");
    ImGui::TextColored((!nes->cpu.N) ? COLOR_WHITE : COLOR_LIGHTGREEN, "N"); 
    ImGui::SameLine();
    ImGui::TextColored((!nes->cpu.V) ? COLOR_WHITE : COLOR_LIGHTGREEN, "V"); 
    ImGui::SameLine();
    ImGui::TextColored(COLOR_LIGHTGREEN, "U"); 
    ImGui::SameLine();
    ImGui::TextColored((!nes->cpu.B) ? COLOR_WHITE : COLOR_LIGHTGREEN, "B"); 
    ImGui::SameLine();
    ImGui::TextColored((!nes->cpu.D) ? COLOR_WHITE : COLOR_LIGHTGREEN, "D"); 
    ImGui::SameLine();
    ImGui::TextColored((!nes->cpu.I) ? COLOR_WHITE : COLOR_LIGHTGREEN, "I"); 
    ImGui::SameLine();
    ImGui::TextColored((!nes->cpu.Z) ? COLOR_WHITE : COLOR_LIGHTGREEN, "Z"); 
    ImGui::SameLine();
    ImGui::TextColored((!nes->cpu.C) ? COLOR_WHITE : COLOR_LIGHTGREEN, "C"); 

    ImGui::SeparatorText("Stack");
    for (i = nes->cpu.sp; i < 0xff; i++)
        ImGui::Text("%04x: %02x", STACK_BASE + i, 
                                nes->cpu.mem[STACK_BASE + i]);
    ImGui::Text("%04x: %02x", STACK_BASE + i, 
                                nes->cpu.mem[STACK_BASE + i]);

    ImGui::SeparatorText("Disassembler");
    for (i = 0; i < 14; i++) {
        if (i == nes->cache_size)
            ImGui::TextColored(COLOR_LIGHTGREEN, ">%s", gui->instr_table[i]);
        else
            ImGui::Text(" %s", gui->instr_table[i]);
    }
    ImGui::End();
        
    ImGui::Render();
    SDL_RenderSetScale(gui->renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(gui->renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
    SDL_RenderClear(gui->renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(gui->renderer);
}