#pragma once
#include <cstdint>
#include <array>

// ============================================================
// globals.hpp — mirrors the Genesis RAM layout used by s2disasm.
//
// Every variable name and address below corresponds to a symbol
// in s2disasm/s2.asm.  Where the original code reads from an
// absolute RAM address the C++ code reads the matching field.
//
// Original offsets are in the comments so you can cross-reference
// the assembly while translating routines.
// ============================================================

// ---------- Game state flags ---------------------------------
// $FFFFF600
extern uint8_t  f_gameMode;      // current game mode index (title/game/etc.)
extern uint8_t  f_gameType;      // 0 = 1P, 1 = 2P vs
extern uint16_t f_frameCount;    // global frame counter ($FFFFF604)
extern uint8_t  f_lives;         // remaining lives ($FFFFF626)
extern uint8_t  f_rings;         // ring count (BCD, $FFFFF628)
extern uint32_t f_score;         // score (BCD, $FFFFF62C)
extern uint16_t f_time_min;      // stage timer minutes ($FFFFF632)
extern uint16_t f_time_sec;      // seconds
extern uint16_t f_time_frame;    // frames
extern uint8_t  f_invincible;    // invincibility timer ($FFFFF648)
extern uint8_t  f_speedShoes;    // speed-shoes timer ($FFFFF64A)
extern uint8_t  f_water;         // in-water flag ($FFFFF65D)
extern uint8_t  f_zoneAct;       // zone+act packed byte ($FFFFF672)

// ---------- Camera / scroll ----------------------------------
// $FFFFF700
extern int16_t  v_screenX;       // left edge of screen in level (pixels)
extern int16_t  v_screenY;       // top  edge of screen in level (pixels)
extern int16_t  v_screenX2;      // screen X sub-pixel (8.8)
extern int16_t  v_planeAHScroll; // Plane A horizontal scroll ($FFFFF710)
extern int16_t  v_planeBHScroll; // Plane B horizontal scroll ($FFFFF712)
extern int16_t  v_planeAVScroll; // Plane A vertical scroll   ($FFFFF714)
extern int16_t  v_planeBVScroll; // Plane B vertical scroll   ($FFFFF716)

// ---------- VDP shadow copy (updated each frame) -------------
// The game keeps a CPU-side mirror of frequently written VDP state.
extern uint8_t  v_vdpRegs[24];

// ---------- Level geometry -----------------------------------
extern uint16_t v_levelWidth;    // level width  in pixels
extern uint16_t v_levelHeight;   // level height in pixels
extern int16_t  v_levelBound_L;  // left boundary (usually 0)
extern int16_t  v_levelBound_R;  // right boundary
extern int16_t  v_levelBound_T;  // top boundary
extern int16_t  v_levelBound_B;  // bottom kill plane

// ---------- Object RAM (mirrors $FFFFE000 – $FFFFFFFF) -------
// Each object slot is 64 bytes in the original.  We represent each
// as a struct; see object.hpp for the full definition.

struct Object;
static constexpr int OBJ_SLOTS = 96;  // same as original
extern std::array<Object, OBJ_SLOTS> v_objects;

// Convenience aliases for named slots (same as s2disasm equates)
// v_player  = v_objects[0]  ($FFFFD000)
// v_player2 = v_objects[1]  ($FFFFD040)
static constexpr int OBJ_SLOT_PLAYER  = 0;
static constexpr int OBJ_SLOT_PLAYER2 = 1;

// ---------- Ring data ----------------------------------------
extern uint16_t v_ringCount;     // rings on screen
