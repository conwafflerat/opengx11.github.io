#pragma once
#include "../game/level.hpp"
#include <cstdint>

// ============================================================
// Emerald Hill Zone — all non-art data extracted from s2disasm.
//
// Source cross-references:
//   _inc/EmeraldHill/Chunks.bin      → ehz_chunks / ehz_chunk_count
//   _inc/EmeraldHill/Layout.bin      → ehz_layout_act1 / ehz_layout_act2
//   _inc/EmeraldHill/Collision1.bin  → ehz_block_col1 (solid layer)
//   _inc/EmeraldHill/Collision2.bin  → ehz_block_col2 (alternate layer)
//   _inc/EmeraldHill/Palette.bin     → ehz_palette (64 Genesis 9-bit words)
//   _inc/EmeraldHill/Object1Layout.bin → ehz_objects_act1
//   _inc/EmeraldHill/Object2Layout.bin → ehz_objects_act2
//   _inc/EmeraldHill/Ring1Layout.bin   → ehz_rings_act1
//   _inc/EmeraldHill/Ring2Layout.bin   → ehz_rings_act2
//
// Collision tile height format:
//   height[col] — number of solid pixels from the bottom of the 16-px cell
//   0  = no solid pixels in this column (pass-through)
//   16 = fully solid column
//
// Genesis colour format: 0000 RRR0 GGG0 BBB0
//   bits 11:9 = Red,  7:5 = Green,  3:1 = Blue
// ============================================================

// ---- Palette (4 lines × 16 colours = 64 entries) ----------------------
// Line 0: sky / water
// Line 1: Sonic + foreground art
// Line 2: secondary foreground
// Line 3: HUD / special items
extern const uint16_t ehz_palette[64];

// ---- Level layout: 2D grid of chunk indices ---------------------------
// Each entry = index into ehz_chunks[]
// Layout is stored row-major, width×height in chunks (each 128×128 px)

static constexpr int EHZ_ACT1_W = 106;  // chunks wide
static constexpr int EHZ_ACT1_H =  8;   // chunks tall

extern const uint8_t ehz_layout_act1[EHZ_ACT1_H][EHZ_ACT1_W];

// ---- Chunk definitions (128×128 px = 8×8 blocks of 16×16 px each) ---
static constexpr int EHZ_CHUNK_COUNT  = 64;
static constexpr int EHZ_CHUNK_BLOCKS = 8;   // 8×8 blocks per chunk (= CHUNK_BLOCKS from level.hpp)

// ehz_chunks[chunk_idx][row][col] = block index
extern const uint8_t ehz_chunks[EHZ_CHUNK_COUNT][EHZ_CHUNK_BLOCKS][EHZ_CHUNK_BLOCKS];

// ---- Block definitions (16×16 px = 2×2 tiles of 8×8 px each) ---------
// Each of the 4 tile slots is a nametable-format word:
//   bit 15 = priority, bits 14:13 = palette, bit 12 = vflip,
//   bit 11 = hflip,  bits 10:0 = tile index in VRAM
static constexpr int EHZ_BLOCK_COUNT = 256;
extern const uint16_t ehz_blocks[EHZ_BLOCK_COUNT][4];  // tl,tr,bl,br

// ---- Collision index: block → collision tile -------------------------
// One byte per block; indexes into ehz_col_tiles[]
extern const uint8_t ehz_block_col[EHZ_BLOCK_COUNT];

// ---- Collision tiles -------------------------------------------------
// Shapes shared across many Sonic games; angles in 0–255 (full circle)
static constexpr int EHZ_COL_TILE_COUNT = 64;
extern const CollisionTile ehz_col_tiles[EHZ_COL_TILE_COUNT];

// ---- Ring placement tables ------------------------------------------
struct RingSpawn {
    int16_t x;  // world pixel X
    int16_t y;  // world pixel Y
};
static constexpr int16_t RING_TABLE_END = -1;  // sentinel (x == -1)

extern const RingSpawn ehz_rings_act1[];
extern const RingSpawn ehz_rings_act2[];

// ---- Object placement tables ----------------------------------------
enum class ObjType : uint8_t {
    Buzzbomber  = 0x01,
    Crabmeat    = 0x02,
    Motobug     = 0x03,
    Coconuts    = 0x04,
    SpeedShoes  = 0x10,  // monitors
    Shield      = 0x11,
    Invincible  = 0x12,
    ExtraLife   = 0x13,
    RingBonus   = 0x14,
    Checkpoint  = 0x20,
    SpringYellow= 0x30,
    SpringRed   = 0x31,
};

struct ObjSpawn {
    ObjType type;
    int16_t x;
    int16_t y;
    uint8_t extra;       // type-specific param (direction, etc.)
};
static constexpr ObjType OBJ_TABLE_END = (ObjType)0xFF;

extern const ObjSpawn ehz_objects_act1[];
extern const ObjSpawn ehz_objects_act2[];

// ---- Zone-level start positions ------------------------------------
struct StartPos { int16_t x, y; };
extern const StartPos ehz_start_act1;
extern const StartPos ehz_start_act2;

// ---- Boundary data -------------------------------------------------
struct ZoneBounds {
    int16_t left, right, top, bottom;
};
extern const ZoneBounds ehz_bounds_act1;
extern const ZoneBounds ehz_bounds_act2;

// ---- Art pointers (ROM offsets for Kosinski-compressed tiles) ------
// These are the byte offsets into the ROM to pass to kosinski_decomp().
// Values from s2disasm's Art_Tiles_* and Art_Blocks_* tables.
static constexpr uint32_t EHZ_ART_TILES_ROM_OFFSET  = 0x3601E;
static constexpr uint32_t EHZ_ART_BLOCKS_ROM_OFFSET = 0x080000;  // placeholder
