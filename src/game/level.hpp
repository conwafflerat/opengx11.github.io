#pragma once
#include <cstdint>
#include <vector>
#include "../hardware/vdp.hpp"

// ============================================================
// level.hpp — tilemap structures and loader.
//
// Genesis Sonic 2 uses a 3-level hierarchy:
//   Chunks  (128×128 px) — 16×16 blocks arranged in a grid
//   Blocks  (  16×16 px) — 2×2 tiles
//   Tiles   (   8×8  px) — 4bpp pattern stored in VRAM
//
// The level layout is a 2D array of chunk indices.
// Collision data is stored per-block (height arrays for each angle).
// ============================================================

// Maximum level dimensions in chunks
static constexpr int MAX_CHUNKS_X = 256;
static constexpr int MAX_CHUNKS_Y = 32;

// Chunk = 16×16 block indices
static constexpr int BLOCKS_PER_CHUNK = 16 * 16;

// A block = 2×2 tile entries (same format as nametable entries)
struct BlockEntry {
    uint16_t tile[4];   // tl, tr, bl, br — same bitfield as NT_* flags above
};

// Chunk = 8×8 block references (each block is 16×16 px → chunk is 128×128 px)
static constexpr int CHUNK_BLOCKS = 8;
struct Chunk {
    uint8_t block[CHUNK_BLOCKS][CHUNK_BLOCKS];
};

// Floor collision map for one block (8 heights, one per pixel column)
struct CollisionTile {
    int8_t  height[16];  // floor height (positive = pixels from bottom)
    uint8_t angle;       // surface angle (0–$FF = full circle)
    uint8_t flags;       // bit 0 = solid top, bit 1 = solid sides, bit 2 = solid bottom
};

class Level {
public:
    bool load(const uint8_t *rom, int zone, int act);
    void unload();

    // Upload art/tilemaps to VDP
    void upload_to_vdp(VDP &vdp) const;

    // Scroll update: redraws the column/row strips that scrolled into view.
    // Called once per frame with the new camera position.
    void update_scroll(VDP &vdp, int cam_x, int cam_y,
                       int prev_cam_x, int prev_cam_y);

    // Collision queries used by sonic.cpp
    int16_t floor_at(int world_x, int world_y, uint8_t &out_angle) const;
    int16_t ceiling_at(int world_x, int world_y) const;
    bool    solid_left(int world_x, int world_y) const;
    bool    solid_right(int world_x, int world_y) const;

    int width_px()  const { return m_width_chunks  * 128; }
    int height_px() const { return m_height_chunks * 128; }

private:
    // Layout: m_layout[y][x] = chunk index
    std::vector<std::vector<uint8_t>> m_layout;

    std::vector<Chunk>         m_chunks;
    std::vector<BlockEntry>    m_blocks;
    std::vector<CollisionTile> m_col_tiles;

    int m_width_chunks  = 0;
    int m_height_chunks = 0;

    // ROM pointer — kept only to feed kosinski_decomp() for art.
    // All other data comes from hardcoded arrays in src/data/.
    const uint8_t *m_rom             = nullptr;
    uint32_t       m_art_rom_offset  = 0;

    // Helpers
    const CollisionTile &col_tile_at(int wx, int wy) const;
    BlockEntry           block_at(int wx, int wy) const;
};
