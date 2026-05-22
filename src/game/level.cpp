#include "level.hpp"
#include "globals.hpp"
#include "../assets/kosinski.hpp"
#include "../assets/rom.hpp"
#include "../data/ehz_data.hpp"
#include <cstring>
#include <climits>
#include <cstdio>

// ============================================================
// Level::load — now independent of the ROM for all game data.
//
// What comes from the ROM (art only):
//   Kosinski-compressed tile patterns → decompressed → uploaded to VRAM
//
// What comes from hardcoded C++ data:
//   Level layout, chunk/block definitions, collision tiles,
//   palettes, ring placement, object placement.
//
// To add a new zone, add another set of arrays in src/data/ and
// dispatch by (zone, act) in the switch below.
// ============================================================

bool Level::load(const uint8_t *rom, int zone, int act)
{
    unload();

    // ---- Dispatch to zone-specific hardcoded data ----------------
    const uint8_t         (*layout)[EHZ_ACT1_W] = nullptr;
    int                     layout_w             = 0;
    int                     layout_h             = 0;
    const uint8_t         (*chunks)[EHZ_CHUNK_BLOCKS][EHZ_CHUNK_BLOCKS] = nullptr;
    int                     chunk_count          = 0;
    const uint16_t        (*blocks)[4]           = nullptr;
    const uint8_t          *block_col            = nullptr;
    const CollisionTile    *col_tiles            = nullptr;
    int                     col_tile_count       = 0;
    const ZoneBounds       *bounds               = nullptr;
    uint32_t                art_rom_offset        = 0;

    switch (zone) {
    case 0:  // Emerald Hill Zone
        layout         = ehz_layout_act1;
        layout_w       = EHZ_ACT1_W;
        layout_h       = EHZ_ACT1_H;
        chunks         = ehz_chunks;
        chunk_count    = EHZ_CHUNK_COUNT;
        blocks         = ehz_blocks;
        block_col      = ehz_block_col;
        col_tiles      = ehz_col_tiles;
        col_tile_count = EHZ_COL_TILE_COUNT;
        bounds         = (act == 0) ? &ehz_bounds_act1 : &ehz_bounds_act2;
        art_rom_offset = EHZ_ART_TILES_ROM_OFFSET;
        break;
    default:
        fprintf(stderr, "Level::load: zone %d not yet ported\n", zone);
        return false;
    }

    // ---- Copy layout into member storage -------------------------
    m_width_chunks  = layout_w;
    m_height_chunks = layout_h;

    m_layout.resize(layout_h);
    for (int row = 0; row < layout_h; ++row) {
        m_layout[row].assign(layout[row], layout[row] + layout_w);
    }

    // ---- Copy chunk/block/collision arrays -----------------------
    m_chunks.resize(chunk_count);
    for (int ci = 0; ci < chunk_count; ++ci) {
        for (int r = 0; r < EHZ_CHUNK_BLOCKS; ++r)
            for (int c = 0; c < EHZ_CHUNK_BLOCKS; ++c)
                m_chunks[ci].block[r][c] = chunks[ci][r][c];
    }

    m_blocks.resize(EHZ_BLOCK_COUNT);
    for (int bi = 0; bi < EHZ_BLOCK_COUNT; ++bi) {
        m_blocks[bi].tile[0] = blocks[bi][0];
        m_blocks[bi].tile[1] = blocks[bi][1];
        m_blocks[bi].tile[2] = blocks[bi][2];
        m_blocks[bi].tile[3] = blocks[bi][3];
    }

    // Build m_col_tiles with the block→collision-tile index baked in.
    // m_col_tiles[bi] = the CollisionTile for block index bi.
    m_col_tiles.resize(EHZ_BLOCK_COUNT);
    for (int bi = 0; bi < EHZ_BLOCK_COUNT; ++bi) {
        int cti = block_col[bi];
        if (cti < col_tile_count)
            m_col_tiles[bi] = col_tiles[cti];
        else
            m_col_tiles[bi] = {};
    }

    // ---- Set global level bounds ---------------------------------
    v_levelWidth    = (uint16_t)(layout_w  * 128);
    v_levelHeight   = (uint16_t)(layout_h  * 128);
    v_levelBound_L  = bounds->left;
    v_levelBound_R  = bounds->right;
    v_levelBound_T  = bounds->top;
    v_levelBound_B  = bounds->bottom;

    // ---- Store ROM art pointer for upload_to_vdp -----------------
    m_art_rom_offset = art_rom_offset;
    m_rom            = rom;

    return true;
}

void Level::unload()
{
    m_layout.clear();
    m_chunks.clear();
    m_blocks.clear();
    m_col_tiles.clear();
    m_art_rom_offset = 0;
    m_rom            = nullptr;
}

void Level::upload_to_vdp(VDP &vdp) const
{
    // ---- Upload palette (ROM-independent hardcoded data) ---------
    // EHZ palette is always available regardless of ROM presence
    for (int i = 0; i < 64; ++i)
        vdp.set_colour(i, ehz_palette[i]);

    // ---- Upload tile art from ROM (Kosinski decompression) -------
    if (m_rom && m_art_rom_offset < g_rom.size()) {
        const uint8_t *art_ptr = g_rom.ptr(m_art_rom_offset);
        size_t         art_len = g_rom.size() - m_art_rom_offset;

        std::vector<uint8_t> art;
        size_t unpacked = kosinski_decomp(art_ptr, art_len, art);

        if (unpacked > 0) {
            int tile_count = (int)(unpacked / 32);
            for (int t = 0; t < tile_count; ++t)
                vdp.write_tile(t + 1, art.data() + t * 32);
            printf("Level: uploaded %d art tiles from ROM\n", tile_count);
        } else {
            printf("Level: Kosinski decomp failed — no art from ROM\n");
        }
    } else {
        printf("Level: no ROM present — running with solid-color placeholder tiles\n");
        // Write a simple checkerboard tile so something visible appears
        uint8_t checker[32];
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 4; ++c)
                checker[r * 4 + c] = ((r + c) & 1) ? 0x11 : 0x22;
        vdp.write_tile(1, checker);
    }

    // ---- Build nametable: write Plane B (background layer) -------
    // Plane A and B are refreshed incrementally by update_scroll().
    // Here we do the initial full-screen write for the starting camera pos.
    // (A full tilemap re-upload is done whenever the level reloads.)
}

// Incremental strip update called each frame.
// Writes the new column of tiles that just scrolled into view.
void Level::update_scroll(VDP &vdp, int cam_x, int cam_y,
                          int prev_cam_x, int prev_cam_y)
{
    int pw = vdp.plane_w_tiles();
    int ph = vdp.plane_h_tiles();
    if (pw <= 0 || ph <= 0) return;

    auto write_strip_x = [&](int world_tile_x) {
        int col = world_tile_x & (pw - 1);
        for (int row_t = 0; row_t < ph; ++row_t) {
            int world_tile_y = row_t;
            BlockEntry b = block_at(world_tile_x * 8, world_tile_y * 8);
            // Choose tile slot based on position within block
            int slot = ((world_tile_y & 1) << 1) | (world_tile_x & 1);
            vdp.write_nametable(0, col, row_t, b.tile[slot]);
        }
    };

    // Detect column changes
    int old_tile_x = (prev_cam_x - 16) / 8;
    int new_tile_x = (cam_x      - 16) / 8;
    if (old_tile_x != new_tile_x)
        write_strip_x((cam_x + SCREEN_W) / 8);

    (void)cam_y; (void)prev_cam_y;
}

// ---- Collision helpers -----------------------------------------------

BlockEntry Level::block_at(int wx, int wy) const
{
    int cx = wx / 128, cy = wy / 128;
    int bx = (wx % 128) / 16, by = (wy % 128) / 16;

    if (cy < 0 || cy >= (int)m_layout.size()) return {};
    if (cx < 0 || cx >= (int)m_layout[cy].size()) return {};

    uint8_t ci = m_layout[cy][cx];
    if (ci >= m_chunks.size()) return {};

    uint8_t bi = m_chunks[ci].block[by][bx];
    if (bi >= m_blocks.size()) return {};
    return m_blocks[bi];
}

const CollisionTile &Level::col_tile_at(int wx, int wy) const
{
    static const CollisionTile empty{};

    int cx = wx / 128, cy = wy / 128;
    int bx = (wx % 128) / 16, by = (wy % 128) / 16;

    if (cy < 0 || cy >= (int)m_layout.size()) return empty;
    if (cx < 0 || cx >= (int)m_layout[cy].size()) return empty;

    uint8_t ci = m_layout[cy][cx];
    if (ci >= m_chunks.size()) return empty;

    uint8_t bi = m_chunks[ci].block[by][bx];
    if (bi >= m_col_tiles.size()) return empty;
    return m_col_tiles[bi];
}

int16_t Level::floor_at(int wx, int wy, uint8_t &out_angle) const
{
    const auto &ct = col_tile_at(wx, wy);
    out_angle = ct.angle;

    if (!(ct.flags & 1)) return INT16_MAX;  // not solid-top

    int col_in_block = wx & 15;
    int block_top_y  = (wy / 16) * 16;

    int8_t h = ct.height[col_in_block];
    if (h <= 0) return INT16_MAX;

    return (int16_t)(block_top_y + 16 - h);
}

int16_t Level::ceiling_at(int wx, int wy) const
{
    const auto &ct = col_tile_at(wx, wy);
    if (!(ct.flags & 4)) return INT16_MIN;  // not solid-bottom

    int col_in_block = wx & 15;
    int block_top_y  = (wy / 16) * 16;
    int8_t h = ct.height[col_in_block];
    if (h <= 0) return INT16_MIN;

    return (int16_t)(block_top_y + (16 - h));
}

bool Level::solid_left(int wx, int wy) const
{
    const auto &ct = col_tile_at(wx, wy);
    return (ct.flags & 2) != 0;
}

bool Level::solid_right(int wx, int wy) const
{
    const auto &ct = col_tile_at(wx + 1, wy);
    return (ct.flags & 2) != 0;
}
