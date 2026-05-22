#pragma once
#include "object.hpp"
#include <cstdint>

// ============================================================
// sonic.hpp — Sonic's physics constants and movement API.
//
// All constants are taken directly from s2disasm.
// Label names match the assembly equates so cross-referencing
// is easy.  The implementation (sonic.cpp) translates each
// subroutine label-for-label.
// ============================================================

// ---------- Physics constants (from s2disasm equates) --------
// Stored as 8.8 fixed-point (low byte = sub-pixel)
static constexpr int16_t SONIC_TOP_SPEED     = 0x0600;  // $06.00 px/frame
static constexpr int16_t SONIC_ACCELERATION  = 0x0046;  // $00.46
static constexpr int16_t SONIC_DECELERATION  = 0x0180;  // $01.80
static constexpr int16_t SONIC_AIR_ACCEL     = 0x0096;  // $00.96 (air acceleration)
static constexpr int16_t SONIC_AIR_DECEL     = 0x0008;  // $00.08 (air drag)
static constexpr int16_t SONIC_JUMP_STRENGTH = 0x0680;  // $06.80 (initial jump vel)
static constexpr int16_t SONIC_GRAVITY       = 0x0038;  // $00.38 (gravity per frame)
static constexpr int16_t SONIC_MAX_FALL      = 0x1000;  // $10.00 (terminal velocity)

// Water variants (half-speed)
static constexpr int16_t SONIC_TOP_SPEED_W   = 0x0300;
static constexpr int16_t SONIC_JUMP_W        = 0x0380;
static constexpr int16_t SONIC_GRAVITY_W     = 0x001C;

// Super Sonic multipliers
static constexpr int16_t SUPER_TOP_SPEED     = 0x0C00;
static constexpr int16_t SUPER_ACCELERATION  = 0x0008;

// ---------- Animation IDs (from s2disasm AnimationIDs) -------
enum class SonicAnim : uint8_t {
    Walk    = 0,
    Run     = 1,
    Roll    = 2,
    Push    = 3,
    Idle    = 4,
    Balance = 5,
    Look_Up = 6,
    Duck    = 7,
    Spindash= 8,
    Hurt    = 9,
    Die     = 10,
    Drown   = 11,
    Jump    = 12,
    Super   = 13,
};

// ---------- Sonic-specific scratch field layout --------------
// Lives inside Object::scratch[32]
struct SonicData {
    uint16_t jumpLock;      // [0] prevents double-jump
    int16_t  groundSpeed;   // [2] speed along ground (8.8 fixed)
    uint8_t  spindashRev;   // [4] spindash charge count
    uint8_t  rollLock;      // [5] prevents un-rolling mid-air
    uint8_t  invTimer;      // [6] invincibility blink counter
    uint8_t  hurtLock;      // [7] hurt state countdown
    SonicAnim anim;         // [8] current animation
    uint8_t  superFlag;     // [9] super sonic active
    uint8_t  shieldType;    // [10] 0=none 1=normal 2=fire 3=elec 4=bubble
};
static_assert(sizeof(SonicData) <= 32, "SonicData overflows Object::scratch");

// ---------- Public API ---------------------------------------
void sonic_init(Object &obj);         // sets up the object, called once
void sonic_update(Object &obj);       // called once per frame
void sonic_hurt(Object &obj);         // take damage
void sonic_kill(Object &obj);         // death sequence
void sonic_collect_ring(Object &obj); // increment ring count + sfx

// Helpers used by level.cpp for floor/ceiling/wall detection
// Returns the Y-floor height at (px,py) in the current level.
// Returns INT16_MAX if no solid floor found.
int16_t level_floor_height(int px, int py, uint8_t &out_angle);
int16_t level_ceiling_height(int px, int py);
bool    level_left_wall(int px, int py);
bool    level_right_wall(int px, int py);
