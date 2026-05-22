#include "main_loop.hpp"
#include "globals.hpp"
#include "object.hpp"
#include "sonic.hpp"
#include "level.hpp"
#include "../assets/rom.hpp"

static Level g_level;

// ---- mode_gameplay --------------------------------------------------
// Mirrors the main game loop from s2disasm (after VBlank_Int ends):
//   1. Run all objects
//   2. Update scroll
//   3. Update VDP scroll registers
//   4. Present frame
void mode_gameplay(GameContext &ctx)
{
    static bool first = true;
    if (first) {
        first = false;

        int zone = (f_zoneAct >> 4) & 0xF;
        int act  =  f_zoneAct       & 0xF;
        g_level.load(g_rom.data(), zone, act);
        g_level.upload_to_vdp(ctx.vdp);

        // Place Sonic at level start
        // TODO: read actual start position from ROM's start-position table
        Object &sonic = v_objects[OBJ_SLOT_PLAYER];
        sonic_init(sonic);
        sonic.x = 200 << 8;
        sonic.y = 100 << 8;
    }

    // ---- Process input ------------------------------------------
    // Translate controller state → sonic inputs.
    // This is the equivalent of s2disasm's input reading at the top
    // of the game loop (after the VBlank wait).
    const auto &p1 = ctx.input.p1();
    Object &sonic = v_objects[OBJ_SLOT_PLAYER];
    auto &sd = *reinterpret_cast<SonicData *>(sonic.scratch);

    // Move left/right
    if (p1.held & BTN_RIGHT) {
        if (sonic.status & ST_ONGROUND)
            sd.groundSpeed = std::min(sd.groundSpeed + SONIC_ACCELERATION,
                                      SONIC_TOP_SPEED);
        else
            sonic.xvel = std::min((int16_t)(sonic.xvel + SONIC_AIR_ACCEL), SONIC_TOP_SPEED);
        sonic.status &= ~ST_XFLIP;
    } else if (p1.held & BTN_LEFT) {
        if (sonic.status & ST_ONGROUND)
            sd.groundSpeed = std::max(sd.groundSpeed - SONIC_ACCELERATION,
                                      (int16_t)-SONIC_TOP_SPEED);
        else
            sonic.xvel = std::max((int16_t)(sonic.xvel - SONIC_AIR_ACCEL),
                                  (int16_t)-SONIC_TOP_SPEED);
        sonic.status |= ST_XFLIP;
    }

    // Jump
    if ((p1.pressed & BTN_B) && (sonic.status & ST_ONGROUND)) {
        // Translated from: Sonic_Jump in s2disasm
        sonic.yvel = -(f_water ? SONIC_JUMP_W : SONIC_JUMP_STRENGTH);
        sonic.status |= ST_JUMPING;
        sonic.status &= ~ST_ONGROUND;
        sd.jumpLock = 1;
    }

    // ---- Run objects --------------------------------------------
    // Mirrors Obj_Run: jsr (a1)+ through the object table.
    for (auto &obj : v_objects) {
        if (obj.active()) obj.update(obj);
    }

    // ---- Scroll update ------------------------------------------
    int prev_x = v_screenX, prev_y = v_screenY;
    g_level.update_scroll(ctx.vdp, v_screenX, v_screenY, prev_x, prev_y);

    // ---- Present -------------------------------------------------
    ctx.vdp.present_frame(v_planeAHScroll, v_planeBHScroll,
                          v_planeAVScroll, v_planeBVScroll);
    ctx.sound.submit_frame();
}

// ---- mode_sega_logo -------------------------------------------------
// TODO: display SEGA logo, play intro jingle, advance to title after delay.
void mode_sega_logo(GameContext &ctx)
{
    static int timer = 0;
    if (++timer > 180) {  // 3 seconds at 60fps
        timer = 0;
        f_gameMode = (uint8_t)GameMode::Title;
    }
    (void)ctx;
}

// ---- mode_title -----------------------------------------------------
// TODO: render title screen art, handle Start → GameMode::Game.
void mode_title(GameContext &ctx)
{
    const auto &p1 = ctx.input.p1();
    if (p1.pressed & BTN_START) {
        f_gameMode = (uint8_t)GameMode::Game;
        f_zoneAct  = 0x00;  // EHZ act 1
        f_lives    = 3;
        f_rings    = 0;
    }
    ctx.vdp.present_frame(0, 0, 0, 0);
}

// ---- Other modes (stubs, fill in from s2disasm) --------------------
void mode_special  (GameContext &ctx) { ctx.vdp.present_frame(0,0,0,0); }
void mode_level_end(GameContext &ctx) { ctx.vdp.present_frame(0,0,0,0); }
void mode_game_over(GameContext &ctx) { ctx.vdp.present_frame(0,0,0,0); }

// ---- Main dispatcher ------------------------------------------------
void game_run_frame(GameContext &ctx)
{
    ctx.input.end_frame();  // latch pressed bits

    switch ((GameMode)f_gameMode) {
    case GameMode::Sega:     mode_sega_logo(ctx);   break;
    case GameMode::Title:    mode_title(ctx);        break;
    case GameMode::Game:     mode_gameplay(ctx);     break;
    case GameMode::SpecStage: mode_special(ctx);     break;
    case GameMode::LevelEnd: mode_level_end(ctx);    break;
    case GameMode::GameOver: mode_game_over(ctx);    break;
    default:
        f_gameMode = (uint8_t)GameMode::Sega;
        break;
    }
}
