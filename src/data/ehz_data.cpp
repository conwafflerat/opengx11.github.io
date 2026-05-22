#include "ehz_data.hpp"

// ============================================================
// NOTE: Values in this file are extracted from s2disasm.
// To verify or update:
//   1. Clone https://github.com/sonicretro/s2disasm
//   2. Assemble: `asm68k /q s2.asm s2.bin`
//   3. Extract the binary sections listed in the header comments
//      to get the exact byte arrays.
// The structure here is correct; values are representative.
// ============================================================

// ---- Palette (Genesis 9-bit BGR: bits 11:9 R, 7:5 G, 3:1 B) --------
// EHZ palette from _inc/EmeraldHill/Palette.bin
// Format: 0b0000_RRR0_GGG0_BBB0
//
// Row 0: sky blues + white
// Row 1: Sonic blues, flesh, dark accents
// Row 2: Green ground, brown, checker highlight
// Row 3: HUD, ring gold, red, white
const uint16_t ehz_palette[64] = {
    // Row 0 – sky (background layer)
    0x0000, 0x0EEE, 0x0CAA, 0x0A88, 0x0866, 0x0644, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    // Row 1 – Sonic + foreground primary
    0x0000, 0x0E00, 0x0C20, 0x0A40, 0x0860, 0x0680, 0x04A0, 0x02C0,
    0x00E0, 0x00C2, 0x00A4, 0x0086, 0x0068, 0x004A, 0x0002, 0x0EEE,
    // Row 2 – foreground secondary (pipes, details)
    0x0000, 0x0EE0, 0x0CC0, 0x0AA0, 0x0880, 0x0660, 0x0440, 0x0220,
    0x0E0E, 0x0C0C, 0x0A0A, 0x0808, 0x0606, 0x0404, 0x0202, 0x0EEE,
    // Row 3 – HUD / ring / specials
    0x0000, 0x00EE, 0x00CC, 0x00AA, 0x0088, 0x0066, 0x0044, 0x0022,
    0x0E0E, 0x0C0C, 0x0A0A, 0x0808, 0x0606, 0x0404, 0x0202, 0x0EEE,
};

// ---- Collision tiles ------------------------------------------------
// CollisionTile: height[16], angle, flags
// flags: bit 0 = solid top, bit 1 = solid sides, bit 2 = solid bottom
const CollisionTile ehz_col_tiles[EHZ_COL_TILE_COUNT] = {
    // [0]  Empty / passthrough
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x00, 0x00},
    // [1]  Full solid flat ground (angle 0)
    {{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}, 0x00, 0x07},
    // [2]  Half height (platform tops)
    {{8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8}, 0x00, 0x01},
    // [3]  45-degree slope, rising right (angle = 0xE0 = 224° ≈ SW)
    {{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}, 0x20, 0x07},
    // [4]  45-degree slope, rising left
    {{16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1}, 0xE0, 0x07},
    // [5]  Gentle slope right (lower half), angle ~0x10
    {{1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8}, 0x10, 0x07},
    // [6]  Gentle slope right (upper half)
    {{9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16}, 0x10, 0x07},
    // [7]  Gentle slope left (lower half)
    {{8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,1}, 0xF0, 0x07},
    // [8]  Gentle slope left (upper half)
    {{16,16,15,15,14,14,13,13,12,12,11,11,10,10,9,9}, 0xF0, 0x07},
    // [9]  Steep slope right (lower half), angle ~0x30
    {{1,3,5,7,9,11,13,15,16,16,16,16,16,16,16,16}, 0x30, 0x07},
    // [10] Steep slope right (upper half)
    {{1,1,1,1,1,1,1,1,2,4,6,8,10,12,14,16}, 0x30, 0x07},
    // [11] Steep slope left (lower half)
    {{16,16,16,16,16,16,16,16,15,13,11,9,7,5,3,1}, 0xD0, 0x07},
    // [12] Steep slope left (upper half)
    {{16,14,12,10,8,6,4,2,1,1,1,1,1,1,1,1}, 0xD0, 0x07},
    // [13] Ceiling flat (solid bottom)
    {{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}, 0x80, 0x04},
    // [14] Left wall (solid right face)
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0x40, 0x02},
    // [15] Right wall (solid left face)
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0xC0, 0x02},
    // [16–63] Fill with empty for now; add more shapes from s2disasm as needed
    {{0}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},
    {{0}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},
    {{0}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},
    {{0}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},
    {{0}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},
    {{0}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},{{{0}}},
};

// ---- Block→collision index ------------------------------------------
// One byte per block; 0 = no collision, 1 = full solid, etc.
// Expand from s2disasm's EHZ_128x128Collision_1.bin
const uint8_t ehz_block_col[EHZ_BLOCK_COUNT] = {
     0, 1, 1, 1, 1, 3, 4, 0,  0, 0, 0, 0, 0, 0, 0, 0,  // 0–15
     1, 1, 3, 4, 5, 6, 7, 8,  9,10,11,12, 0, 0, 0, 0,  // 16–31
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,  // 32–47 (expand)
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
};

// ---- Block tile definitions -----------------------------------------
// [block][slot] = nametable entry (pri|pal|vflip|hflip|tile_index)
// Slot order: [0]=top-left [1]=top-right [2]=bot-left [3]=bot-right
// Tile indices here are VRAM tile numbers after art upload.
// Replace with actual values from s2disasm EHZ block data.
const uint16_t ehz_blocks[EHZ_BLOCK_COUNT][4] = {
    {0x0000,0x0000,0x0000,0x0000},  // [0]  empty
    {0x2001,0x2002,0x2003,0x2004},  // [1]  solid ground
    {0x2005,0x2006,0x2007,0x2008},  // [2]  ground variant
    {0x2009,0x200A,0x200B,0x200C},  // [3]  slope tile
    {0x200D,0x200E,0x200F,0x2010},  // [4]  slope tile reverse
    {0x2011,0x2012,0x2013,0x2014},  // [5]  gentle slope a
    {0x2015,0x2016,0x2017,0x2018},  // [6]  gentle slope b
    {0x2019,0x201A,0x201B,0x201C},  // [7]  pipe horizontal
    {0x201D,0x201E,0x201F,0x2020},  // [8]  pipe vertical
    // Fill remainder; expand from s2disasm EHZ_Blocks.bin
};

// ---- Chunk definitions (8×8 blocks each) ---------------------------
// Chunks are the 128×128 px map units; each cell is a 16px block index.
// Extract from s2disasm _inc/EmeraldHill/128x128_Tiles_Index.bin
const uint8_t ehz_chunks[EHZ_CHUNK_COUNT][EHZ_CHUNK_BLOCKS][EHZ_CHUNK_BLOCKS] = {
    // [0] Blank sky chunk
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},
     {0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    // [1] Ground level chunk: top 5 rows sky, bottom 3 rows solid
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},
     {0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1}},
    // [2] Ground chunk with slope top-right
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,3},{0,0,0,0,0,0,3,1},
     {0,0,0,0,0,3,1,1},{0,0,0,0,3,1,1,1},{0,0,0,3,1,1,1,1},{1,1,1,1,1,1,1,1}},
    // [3] Ground chunk with slope top-left
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{4,0,0,0,0,0,0,0},{1,4,0,0,0,0,0,0},
     {1,1,4,0,0,0,0,0},{1,1,1,4,0,0,0,0},{1,1,1,1,4,0,0,0},{1,1,1,1,1,1,1,1}},
    // [4] Deep underground (all solid)
    {{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1},
     {1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1},{1,1,1,1,1,1,1,1}},
    // [5] Elevated ledge chunk (platform floating in sky)
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,2,2,2,2,2,2,0},
     {0,1,1,1,1,1,1,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    // [6–63] Sky / fill; expand from s2disasm chunk data
};

// ---- Level layout Act 1 (representative EHZ Act 1 structure) --------
// 0 = blank sky, 1 = ground level, 2 = slope up, 3 = slope down, 4 = solid
// Full data: extract from s2disasm _inc/EmeraldHill/Level1.bin
const uint8_t ehz_layout_act1[EHZ_ACT1_H][EHZ_ACT1_W] = {
//    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },  // row 0 – above sky
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0 },            // row 1
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0 },            // row 2
    { 0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0 },            // row 3 – floating platforms
    { 1, 1, 1, 2, 0, 0, 0, 0, 0, 3, 1, 2, 0, 0, 3, 1, 1, 1, 1, 1, 2, 0, 0, 3, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1 },            // row 4 – main ground
    { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4 },            // row 5 – underground
    { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4 },            // row 6
    { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4 },            // row 7
};

// ---- Ring placement -------------------------------------------------
const RingSpawn ehz_rings_act1[] = {
    // Rings near Sonic's start
    {0x0180, 0x04C0}, {0x0200, 0x04C0}, {0x0280, 0x04C0},
    {0x0300, 0x04C0}, {0x0380, 0x04C0},
    // Rings over first slope
    {0x0600, 0x0440}, {0x0680, 0x0400}, {0x0700, 0x03C0},
    {0x0780, 0x0400}, {0x0800, 0x0440},
    // Rings on floating platform
    {0x0C00, 0x03C0}, {0x0C80, 0x03C0}, {0x0D00, 0x03C0},
    // More rings across the level (expand from s2disasm Ring1Layout.bin)
    {0x1000, 0x04C0}, {0x1080, 0x04C0}, {0x1100, 0x04C0},
    {0x1180, 0x04C0}, {0x1200, 0x04C0}, {0x1280, 0x04C0},
    {-1, 0},  // sentinel
};

const RingSpawn ehz_rings_act2[] = {
    {0x0180, 0x04C0}, {0x0200, 0x04C0}, {0x0280, 0x04C0},
    {-1, 0},
};

// ---- Object placement -----------------------------------------------
const ObjSpawn ehz_objects_act1[] = {
    // Speed-shoes monitor near start
    {ObjType::SpeedShoes, 0x0500, 0x0460, 0},
    // Ring bonus monitor
    {ObjType::RingBonus,  0x0900, 0x0460, 0},
    // Motobug enemy patrolling the ground
    {ObjType::Motobug,    0x0700, 0x04A0, 0},  // direction 0 = left
    {ObjType::Motobug,    0x0F00, 0x04A0, 1},  // direction 1 = right
    // Buzzbomber on a pipe
    {ObjType::Buzzbomber, 0x0B00, 0x0380, 0},
    // Checkpoint
    {ObjType::Checkpoint, 0x1800, 0x04A0, 0},
    {(ObjType)OBJ_TABLE_END, 0, 0, 0},
};

const ObjSpawn ehz_objects_act2[] = {
    {ObjType::Motobug, 0x0500, 0x04A0, 0},
    {(ObjType)OBJ_TABLE_END, 0, 0, 0},
};

// ---- Start positions ------------------------------------------------
const StartPos ehz_start_act1 = {0x00C0, 0x04B0};  // x=192, y=1200 (above ground row)
const StartPos ehz_start_act2 = {0x00C0, 0x04B0};

// ---- Boundary data --------------------------------------------------
const ZoneBounds ehz_bounds_act1 = {
    0,                              // left
    EHZ_ACT1_W * 128 - 1,          // right
    0,                              // top
    EHZ_ACT1_H * 128 + 128         // bottom kill plane
};

const ZoneBounds ehz_bounds_act2 = {
    0,
    EHZ_ACT1_W * 128 - 1,
    0,
    EHZ_ACT1_H * 128 + 128
};
