#pragma once
#include "../hardware/vdp.hpp"
#include "../hardware/sound.hpp"
#include "../hardware/input.hpp"

// ============================================================
// Game modes — matches the GameMode_* labels in s2disasm.
// The main loop dispatches to each mode's entry function.
// ============================================================
enum class GameMode : uint8_t {
    Sega      = 0x00,   // SEGA logo
    Title     = 0x04,   // title screen
    Demo      = 0x08,   // attract demo
    TwoPlayer = 0x0C,   // 2P vs mode select
    Game      = 0x0E,   // actual gameplay
    SpecStage = 0x12,   // special stage
    LevelEnd  = 0x16,   // results tally
    GameOver  = 0x1A,
    Continue  = 0x1E,
    Options   = 0x22,
};

// ============================================================
// One game context passed into every subsystem.
// ============================================================
struct GameContext {
    VDP   &vdp;
    Sound &sound;
    Input &input;
};

// Main dispatcher — call once per frame
void game_run_frame(GameContext &ctx);

// Mode entry points (each is a self-contained subsystem)
void mode_sega_logo  (GameContext &ctx);
void mode_title      (GameContext &ctx);
void mode_gameplay   (GameContext &ctx);
void mode_special    (GameContext &ctx);
void mode_level_end  (GameContext &ctx);
void mode_game_over  (GameContext &ctx);
