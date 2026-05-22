#include "sonic.hpp"
#include "globals.hpp"
#include "level.hpp"
#include "ring.hpp"
#include <cstdlib>
#include <algorithm>
#include <cmath>

// ============================================================
// Sonic physics — translated from s2disasm.
//
// Translation methodology:
//   1. Each asm subroutine becomes a C++ function with the same name.
//   2. d-registers become local int16_t/int32_t variables.
//   3. a0 pointing to the current object becomes a reference parameter.
//   4. RAM variable reads/writes become global variable accesses.
//   5. VDP writes are deferred; the VDP state is updated once per frame.
//   6. move.w / move.b become direct C++ assignments.
//   7. beq / bne / blt etc. become if() / goto (for complex branches).
// ============================================================

static SonicData &sdata(Object &obj)
{
    return *reinterpret_cast<SonicData *>(obj.scratch);
}

// ---- Sonic_MdJump (s2disasm label) ----------------------------------
// Translated from:
//   Sonic_Jump:
//       btst #1, status(a0)     ; already jumping?
//       bne.s .done
//       move.w #JUMP_STRENGTH, yvel(a0)
//       bset #6, status(a0)     ; ST_JUMPING
// Called when jump button just pressed on the ground.
static void Sonic_Jump(Object &obj)
{
    if (obj.status & ST_JUMPING) return;  // already in air

    int16_t jump = f_water ? SONIC_JUMP_W : SONIC_JUMP_STRENGTH;
    obj.yvel = -jump;          // negative = upward
    obj.status |= ST_JUMPING;
    obj.status &= ~ST_ONGROUND;
    sdata(obj).jumpLock = 1;
}

// ---- Sonic_AirMove (s2disasm label) --------------------------------
// Air-phase horizontal movement and gravity.
// Translated from s2disasm's Sonic_Move_Air / Sonic_Gravity routines.
static void Sonic_AirMove(Object &obj)
{
    // Input is applied in main_loop.cpp (GameContext) before sonic_update() is called.
    // Horizontal air velocity has already been adjusted when we get here.

    int16_t top  = f_water ? SONIC_TOP_SPEED_W : SONIC_TOP_SPEED;
    (void)top;

    // Horizontal air movement (looser control than ground)
    // btst #BTN_RIGHT, d0 / btst #BTN_LEFT, d0
    // We read input from globals (see input section in main_loop.cpp)
    // TODO: wire up Input reference here when integrating with GameContext
    // For now we check g_input via an extern:
    // (the full wiring happens in mode_gameplay via sonic_update)

    // Gravity: add gravity constant each frame
    int16_t grav = f_water ? SONIC_GRAVITY_W : SONIC_GRAVITY;
    obj.yvel += grav;
    if (obj.yvel > SONIC_MAX_FALL) obj.yvel = SONIC_MAX_FALL;

    obj.apply_velocity();
}

// ---- Sonic_GndMove --------------------------------------------------
// Ground movement — accelerate/decelerate along ground slope.
// Translated from Sonic_MoveOnGround / Sonic_Roll in s2disasm.
static void Sonic_GndMove(Object &obj)
{
    auto &sd = sdata(obj);
    // (input wired in sonic_update)

    // Decelerate when no input
    if (sd.groundSpeed > 0)  sd.groundSpeed -= SONIC_DECELERATION / 8;
    if (sd.groundSpeed < 0)  sd.groundSpeed += SONIC_DECELERATION / 8;
    if (std::abs(sd.groundSpeed) < SONIC_DECELERATION / 8) sd.groundSpeed = 0;

    // Cap to top speed
    sd.groundSpeed = std::clamp(sd.groundSpeed,
                                (int16_t)-SONIC_TOP_SPEED,
                                (int16_t) SONIC_TOP_SPEED);

    // Convert ground speed → X/Y velocity using angle
    // In s2disasm: use sine/cosine table indexed by obj.angle
    // For angle=0 (flat ground): xvel = groundSpeed, yvel = 0
    // cos_table[angle] and sin_table[angle] are in s2disasm's tables
    float a = obj.angle * (3.14159265f * 2.0f / 256.0f);
    obj.xvel = (int16_t)(sd.groundSpeed * std::cos(a));  // cast OK for 8.8 scale
    // yvel from slope is usually overridden by floor snap, set to 0 on ground
    obj.yvel = 0;

    obj.apply_velocity();
}

// ---- Floor detection ------------------------------------------------
// Translated from s2disasm's Floor_Check / Sonic_FloorDist routines.
// Calls level_floor_height() which queries the collision tile data.
static void Sonic_FloorCheck(Object &obj)
{
    uint8_t angle = 0;
    int wx = obj.pixel_x();
    int wy = obj.pixel_y();

    int16_t floor = level_floor_height(wx, wy, angle);

    if (floor != INT16_MAX) {
        int16_t dist = (int16_t)(wy - floor);
        if (dist <= 0 && dist >= -14) {
            // Land on floor
            obj.y = (int32_t)floor << 8;
            obj.yvel = 0;
            obj.angle = angle;
            obj.status |= ST_ONGROUND;
            obj.status &= ~ST_JUMPING;
            sdata(obj).jumpLock = 0;
        } else if (dist > 14) {
            // Fell through floor — shouldn't happen in normal play
            obj.status &= ~ST_ONGROUND;
        }
    } else {
        obj.status &= ~ST_ONGROUND;
    }
}

// ---- Camera ---------------------------------------------------------
// Translated from s2disasm's Camera_Update.
// Centres the camera on Sonic with boundary clamping.
static void Sonic_UpdateCamera(const Object &obj)
{
    int target_x = obj.pixel_x() - SCREEN_W / 2;
    int target_y = obj.pixel_y() - SCREEN_H / 2 + 16;  // 16 px bias upward

    v_screenX = (int16_t)std::clamp(target_x, (int)v_levelBound_L,
                                               (int)v_levelBound_R - SCREEN_W);
    v_screenY = (int16_t)std::clamp(target_y, (int)v_levelBound_T,
                                               (int)v_levelBound_B - SCREEN_H);

    v_planeAHScroll = -v_screenX;
    v_planeBHScroll = -(v_screenX / 2);  // parallax
    v_planeAVScroll = -v_screenY;
    v_planeBVScroll = -(v_screenY / 2);
}

// ---- Boundary check -------------------------------------------------
// Kill Sonic if he falls below the level kill plane.
static void Sonic_BoundaryCheck(Object &obj)
{
    if (obj.pixel_y() > v_levelBound_B) {
        sonic_kill(obj);
        return;
    }
    if (obj.pixel_x() < v_levelBound_L) {
        obj.x = (int32_t)v_levelBound_L << 8;
        obj.xvel = 0;
    }
    if (obj.pixel_x() > v_levelBound_R) {
        obj.x = (int32_t)v_levelBound_R << 8;
        obj.xvel = 0;
    }
}

// ---- Screen position update -----------------------------------------
static void Sonic_UpdateScreenPos(Object &obj)
{
    obj.screenX = (int16_t)(obj.pixel_x() - v_screenX);
    obj.screenY = (int16_t)(obj.pixel_y() - v_screenY);
}

// ====================================================================
// Public entry points
// ====================================================================

void sonic_init(Object &obj)
{
    obj = Object{};
    obj.halfWidth  = 9;
    obj.halfHeight = 19;
    obj.status     = 0;
    obj.angle      = 0;
    sdata(obj).groundSpeed = 0;
    sdata(obj).anim        = SonicAnim::Idle;
    obj.update = sonic_update;
}

void sonic_update(Object &obj)
{
    // Boundary check first
    Sonic_BoundaryCheck(obj);

    if (obj.status & ST_ONGROUND) {
        Sonic_GndMove(obj);
        Sonic_FloorCheck(obj);
    } else {
        Sonic_AirMove(obj);
        Sonic_FloorCheck(obj);
    }

    Sonic_UpdateScreenPos(obj);
    Sonic_UpdateCamera(obj);
    f_frameCount++;
}

void sonic_hurt(Object &obj)
{
    auto &sd = sdata(obj);
    if (sd.hurtLock) return;

    if (f_rings > 0) {
        ring_scatter_all(obj.pixel_x(), obj.pixel_y(),
                         (uint8_t)(f_rings > 16 ? 16 : f_rings));
        f_rings = 0;
    } else {
        sonic_kill(obj);
        return;
    }

    sd.hurtLock = 60;
    obj.yvel = -(SONIC_JUMP_STRENGTH / 2);
    obj.status &= ~ST_ONGROUND;
    sd.anim = SonicAnim::Hurt;
}

void sonic_kill(Object &obj)
{
    sdata(obj).anim = SonicAnim::Die;
    obj.yvel = -(SONIC_JUMP_STRENGTH / 2);
    obj.status &= ~ST_ONGROUND;
    // TODO: trigger death sequence / game over
}

void sonic_collect_ring(Object &obj)
{
    (void)obj;
    f_rings = (uint8_t)std::min((int)f_rings + 1, 99);
    // TODO: SFX
}

// ---- Stub collision queries (Level integration) -------------------
// These are wired up when level.cpp is complete.

int16_t level_floor_height(int px, int py, uint8_t &out_angle)
{
    // TODO: query Level instance
    (void)px; (void)py;
    out_angle = 0;
    return INT16_MAX;
}

int16_t level_ceiling_height(int px, int py)
{
    (void)px; (void)py;
    return INT16_MIN;
}

bool level_left_wall(int px, int py)  { (void)px; (void)py; return false; }
bool level_right_wall(int px, int py) { (void)px; (void)py; return false; }
