#include "level.hpp"
#include "../assets/rom.hpp"
#include <cstring>
#include <climits>

// ============================================================
// Level loader — reads the Sonic 2 ROM's level data and builds
// the chunk/block/collision tables in CPU-side memory,
// then uploads tile art to the VDP.
//
// All offsets come from s2disasm's LevelData_Table and the
// zone-specific include files (e.g. _inc/EmeraldHill/LevelArt.bin).
// ============================================================

// Zone/act data table from s2disasm (each entry = 8 bytes with
// pointers to art, layout, palette, etc.)
// Adjust these offsets once you know the exact ROM addresses
// by matching against s2disasm's LevelData_Table.
struct ZoneDataEntry {
    uint32_t art_ptr;       // pointer to compressed art
    uint32_t layout_ptr;    // pointer to level layout
    uint32_t chunk_ptr;     // pointer to 128×128 chunk maps
    uint32_t col_ptr;       // pointer to collision tile index
};

bool Level::load(const uint8_t *rom, int zone, int act)
{
    (void)rom; (void)zone; (void)act;
    // TODO: parse the ROM's LevelData_Table, decompress art,
    // load layout grid, load collision data.
    //
    // Typical steps (matching s2disasm's level-load routine):
    //
    //  1. Index into LevelData_Table by (zone*2 + act)
    //  2. Decompress tile art via Kosinski decompression
    //  3. Load the 128×128 chunk definitions
    //  4. Load the level layout (2D array of chunk indices)
    //  5. Load the collision tile index arrays
    //  6. Load the height + angle arrays
    //  7. Set v_levelWidth, v_levelHeight, v_levelBound_*

    // Placeholder geometry so sonic.cpp stubs don't crash:
    m_width_chunks  = 64;
    m_height_chunks = 16;
    return true;
}

void Level::unload()
{
    m_layout.clear();
    m_chunks.clear();
    m_blocks.clear();
    m_col_tiles.clear();
}

void Level::upload_to_vdp(VDP &vdp) const
{
    // TODO: call vdp.write_tile() for each decompressed tile,
    // set palettes via vdp.set_colour(), initialise nametables.
    (void)vdp;
}

void Level::update_scroll(VDP &vdp, int cam_x, int cam_y,
                          int prev_cam_x, int prev_cam_y)
{
    // TODO: incremental strip update — upload new tile column/row
    // as camera moves, mirroring the VDP interrupt-driven strip
    // writes in s2disasm's HBlank_Int / VBlank_Int handlers.
    (void)vdp;
    (void)cam_x; (void)cam_y;
    (void)prev_cam_x; (void)prev_cam_y;
}

BlockEntry Level::block_at(int wx, int wy) const
{
    int cx = wx / 128, cy = wy / 128;
    int bx = (wx % 128) / 16, by = (wy % 128) / 16;

    if (cy >= (int)m_layout.size() || cx >= (int)m_layout[cy].size())
        return {};
    uint8_t ci = m_layout[cy][cx];
    if (ci >= m_chunks.size()) return {};
    return m_blocks[m_chunks[ci].block[by][bx]];
}

const CollisionTile &Level::col_tile_at(int wx, int wy) const
{
    static const CollisionTile empty{};
    int bx = wx / 16, by = wy / 16;
    // TODO: look up collision tile from block_at() result
    (void)bx; (void)by;
    return empty;
}

int16_t Level::floor_at(int wx, int wy, uint8_t &out_angle) const
{
    const auto &ct = col_tile_at(wx, wy);
    out_angle = ct.angle;
    int col = wx & 15;
    int base_y = (wy / 16) * 16;
    int h = ct.height[col];
    if (h <= 0) return INT16_MAX;
    return (int16_t)(base_y + 16 - h);
}

int16_t Level::ceiling_at(int wx, int wy) const
{
    (void)wx; (void)wy;
    return INT16_MIN;
}

bool Level::solid_left(int wx, int wy) const
{
    const auto &ct = col_tile_at(wx - 1, wy);
    return (ct.flags & 2) != 0;
}

bool Level::solid_right(int wx, int wy) const
{
    const auto &ct = col_tile_at(wx + 1, wy);
    return (ct.flags & 2) != 0;
}
