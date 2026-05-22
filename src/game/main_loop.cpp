#include "main_loop.hpp"
#include "globals.hpp"
#include "object.hpp"
#include "sonic.hpp"
#include "level.hpp"
#include "ring.hpp"
#include "monitor.hpp"
#include "sound_driver.hpp"
#include "../assets/rom.hpp"
#include "../data/ehz_data.hpp"

static Level g_level;
static bool  g_sound_driver_ready = false;

// ---- Spawn objects from hardcoded placement tables ------------------
static void spawn_rings(int zone, int act)
{
    const RingSpawn *table = nullptr;
    switch (zone) {
    case 0: table = (act == 0) ? ehz_rings_act1 : ehz_rings_act2; break;
    default: return;
    }
    for (; table->x != RING_TABLE_END; ++table) {
        Object *slot = object_alloc();
        if (!slot) break;
        ring_init(*slot, table->x, table->y);
    }
}

static void spawn_objects(int zone, int act)
{
    const ObjSpawn *table = nullptr;
    switch (zone) {
    case 0: table = (act == 0) ? ehz_objects_act1 : ehz_objects_act2; break;
    default: return;
    }
    for (; table->type != OBJ_TABLE_END; ++table) {
        Object *slot = object_alloc();
        if (!slot) continue;

        switch (table->type) {
        case ObjType::SpeedShoes:
            monitor_init(*slot, MonitorItem::SpeedShoes, table->x, table->y);
            break;
        case ObjType::Shield:
            monitor_init(*slot, MonitorItem::Shield,     table->x, table->y);
            break;
        case ObjType::Invincible:
            monitor_init(*slot, MonitorItem::Invincible, table->x, table->y);
            break;
        case ObjType::ExtraLife:
            monitor_init(*slot, MonitorItem::ExtraLife,  table->x, table->y);
            break;
        case ObjType::RingBonus:
            monitor_init(*slot, MonitorItem::Rings,      table->x, table->y);
            break;
        default:
            // Other object types (enemies, springs, checkpoints) — add here
            slot->deactivate();
            break;
        }
    }
}

// ---- mode_gameplay --------------------------------------------------
void mode_gameplay(GameContext &ctx)
{
    static bool first_frame = true;
    if (first_frame) {
        first_frame = false;

        int zone = (f_zoneAct >> 4) & 0xF;
        int act  =  f_zoneAct       & 0xF;

        // Load level — ROM is optional (art only)
        g_level.load(g_rom.loaded() ? g_rom.data() : nullptr, zone, act);
        g_level.upload_to_vdp(ctx.vdp);

        // Clear all object slots before spawning
        for (auto &o : v_objects) o.deactivate();

        // Place Sonic in slot 0 at the zone start position
        const StartPos &sp = (zone == 0 && act == 0)
            ? ehz_start_act1 : ehz_start_act2;
        Object &sonic = v_objects[OBJ_SLOT_PLAYER];
        sonic_init(sonic);
        sonic.x = (int32_t)sp.x << 8;
        sonic.y = (int32_t)sp.y << 8;

        // Spawn rings and objects from hardcoded placement tables
        spawn_rings(zone, act);
        spawn_objects(zone, act);

        // Start BGM
        if (!g_sound_driver_ready) {
            sound_driver_init(ctx.sound);
            g_sound_driver_ready = true;
        }
        sound_driver_play_bgm(zone == 0 ? BGM::EmeraldHill : BGM::None);
    }

    // ---- Process input (apply to Sonic before physics update) -----
    const auto &p1 = ctx.input.p1();
    Object &sonic  = v_objects[OBJ_SLOT_PLAYER];
    auto   &sd     = *reinterpret_cast<SonicData *>(sonic.scratch);

    if (sonic.active()) {
        if (p1.held & BTN_RIGHT) {
            if (sonic.status & ST_ONGROUND)
                sd.groundSpeed = std::min(sd.groundSpeed + SONIC_ACCELERATION,
                                          SONIC_TOP_SPEED);
            else
                sonic.xvel = std::min((int16_t)(sonic.xvel + SONIC_AIR_ACCEL),
                                      SONIC_TOP_SPEED);
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

        // Jump (newly pressed only)
        if ((p1.pressed & BTN_B) && (sonic.status & ST_ONGROUND)) {
            sonic.yvel = -(f_water ? SONIC_JUMP_W : SONIC_JUMP_STRENGTH);
            sonic.status |=  ST_JUMPING;
            sonic.status &= ~ST_ONGROUND;
            sd.jumpLock = 1;
            sound_driver_play_sfx(SFX::Jump);
        }
    }

    // ---- Run all active objects ----------------------------------
    int prev_cam_x = v_screenX;
    int prev_cam_y = v_screenY;

    for (auto &obj : v_objects) {
        if (obj.active() && obj.update) obj.update(obj);
    }

    // ---- Update timer -------------------------------------------
    if (++f_time_frame >= 60) {
        f_time_frame = 0;
        if (++f_time_sec >= 60) {
            f_time_sec = 0;
            ++f_time_min;
        }
    }

    // ---- Scroll update ------------------------------------------
    g_level.update_scroll(ctx.vdp, v_screenX, v_screenY,
                          prev_cam_x, prev_cam_y);

    // ---- Sound driver tick (advances notes, writes YM2612 regs) -
    sound_driver_tick(ctx.sound);

    // ---- Present frame ------------------------------------------
    ctx.vdp.present_frame(v_planeAHScroll, v_planeBHScroll,
                          v_planeAVScroll, v_planeBVScroll);
    ctx.sound.submit_frame();
}

// ---- mode_sega_logo -------------------------------------------------
void mode_sega_logo(GameContext &ctx)
{
    static int timer = 0;
    if (++timer > 180) {
        timer = 0;
        f_gameMode = (uint8_t)GameMode::Title;
    }
    ctx.vdp.present_frame(0, 0, 0, 0);
}

// ---- mode_title -----------------------------------------------------
void mode_title(GameContext &ctx)
{
    const auto &p1 = ctx.input.p1();
    if (p1.pressed & BTN_START) {
        f_gameMode = (uint8_t)GameMode::Game;
        f_zoneAct  = 0x00;   // EHZ Act 1
        f_lives    = 3;
        f_rings    = 0;
        f_score    = 0;
        f_time_min = f_time_sec = f_time_frame = 0;
    }
    ctx.vdp.present_frame(0, 0, 0, 0);
}

// ---- Mode stubs (implement each from s2disasm) ----------------------
void mode_special  (GameContext &ctx) { ctx.vdp.present_frame(0,0,0,0); }
void mode_level_end(GameContext &ctx) { ctx.vdp.present_frame(0,0,0,0); }
void mode_game_over(GameContext &ctx) { ctx.vdp.present_frame(0,0,0,0); }

// ---- Main dispatcher ------------------------------------------------
void game_run_frame(GameContext &ctx)
{
    ctx.input.end_frame();

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
