#include <SDL.h>
#include <cstdio>

#include "hardware/vdp.hpp"
#include "hardware/sound.hpp"
#include "hardware/input.hpp"
#include "game/globals.hpp"
#include "game/main_loop.hpp"
#include "assets/rom.hpp"

int main(int argc, char **argv)
{
    const char *rom_path = (argc >= 2) ? argv[1] : "sonic2.md";

    if (!g_rom.load(rom_path)) {
        fprintf(stderr, "Usage: sonic2 <sonic2.md>\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Sonic the Hedgehog 2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * 3, SCREEN_H * 3,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, 0);

    VDP   vdp;
    Sound sound;
    Input input;

    if (!vdp.init(renderer)) {
        fprintf(stderr, "VDP init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!sound.init())
        fprintf(stderr, "Warning: audio unavailable: %s\n", SDL_GetError());

    input.open_controller();

    f_gameMode = (uint8_t)GameMode::Sega;

    GameContext ctx{vdp, sound, input};

    const uint32_t FRAME_MS = 1000 / 60;
    bool running = true;

    while (running) {
        uint32_t t0 = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = false; break; }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) { running = false; break; }
            input.process_event(ev);
        }

        game_run_frame(ctx);

        uint32_t elapsed = SDL_GetTicks() - t0;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }

    input.close_controller();
    sound.shutdown();
    vdp.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
