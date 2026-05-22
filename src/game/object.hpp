#pragma once
#include <cstdint>
#include <functional>

// ============================================================
// Object RAM layout — matches the s2disasm object structure.
//
// In the original each object occupies 64 bytes of Genesis RAM
// starting at $FFFFD000.  Offsets are preserved as comments so
// assembly-to-C++ translation is straightforward.
//
// Positions use the Genesis 16.16-bit fixed-point format:
//   high word = integer pixel position
//   low  word = sub-pixel (0x0100 = 1 pixel)
// ============================================================

// Routine index for each object update phase
enum class ObjRoutine : uint16_t {
    Init     = 0,
    Main     = 2,
    AirMove  = 4,
    GndMove  = 6,
    Dying    = 8,
    Dead     = 10,
};

// Object status flags (matches s2disasm's obj_status bits)
enum ObjStatus : uint8_t {
    ST_XFLIP       = 0x01,
    ST_YFLIP       = 0x02,
    ST_ONGROUND    = 0x04,
    ST_INWATER     = 0x08,
    ST_ROLL        = 0x10,
    ST_PUSHING     = 0x20,
    ST_JUMPING     = 0x40,
    ST_STANDING_ON = 0x80,
};

// Collision response type
enum class ColType : uint8_t {
    None  = 0,
    Enemy = 1,
    Item  = 2,
    Ring  = 3,
};

struct Object {
    // +$00  routine pointer index (phase of object lifecycle)
    uint16_t routine    = 0;

    // +$02  X position: high word = integer pixels, low word = sub-pixels
    int32_t  x          = 0;   // stored as 16.16 fixed-point (<<8 of s2disasm)
    // +$04  X velocity (16.8 fixed-point, signed; +right / -left)
    int16_t  xvel       = 0;
    // +$06  Y position (16.16 fixed-point)
    int32_t  y          = 0;
    // +$08  Y velocity (16.8 fixed-point, signed; +down / -up)
    int16_t  yvel       = 0;

    // +$0A  screen X (pixel position relative to left edge — updated from camera)
    int16_t  screenX    = 0;
    // +$0C  screen Y
    int16_t  screenY    = 0;

    // +$0E  respawn table entry address (0 = no respawn)
    uint16_t respawnPtr = 0;

    // +$10  collision box half-width (pixels)
    uint8_t  halfWidth  = 8;
    // +$11  collision box half-height
    uint8_t  halfHeight = 8;

    // +$12  object status bitfield (see ObjStatus enum)
    uint8_t  status     = 0;

    // +$13  collision response type
    ColType  colType    = ColType::None;

    // +$14  art tile base | priority | palette | flip
    //        Bit 15 = priority, 13-14 = palette, 12 = vflip, 11 = hflip, 0-10 = tile index
    uint16_t artTile    = 0;

    // +$16  frame/animation pointer offset into sprite map table
    uint16_t mappingPtr = 0;

    // +$18  animation frame counter
    uint8_t  aniFrame   = 0;
    // +$19  animation speed
    uint8_t  aniSpeed   = 0;

    // +$1A  ground angle (0–$FF = 0°–360°, like s2disasm's angle byte)
    uint8_t  angle      = 0;

    // +$1C  object-specific scratch space (32 bytes), use as needed per object type
    uint8_t  scratch[32]{};

    // --------------------------------------------------------
    // Update function pointer — set by Init routine.
    // Replaces the JSR-through-table pattern in the original.
    // --------------------------------------------------------
    using UpdateFn = void(*)(Object &);
    UpdateFn update = nullptr;

    // Helper: pixel X/Y (integer part only)
    int pixel_x() const { return x >> 8; }
    int pixel_y() const { return y >> 8; }

    // Helper: apply velocity (called once per frame inside movement routines)
    void apply_velocity() {
        x += xvel;
        y += yvel;
    }

    // Is this slot in use?
    bool active() const { return update != nullptr; }
    void deactivate()   { update = nullptr; routine = 0; }
};
