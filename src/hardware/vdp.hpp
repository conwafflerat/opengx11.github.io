#pragma once
#include <SDL.h>
#include <cstdint>
#include <array>

// Screen geometry (H40 / V28 mode — what Sonic 2 uses)
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 224;

// Tile dimensions
static constexpr int TILE_W = 8;
static constexpr int TILE_H = 8;
static constexpr int TILES_X = SCREEN_W / TILE_W;   // 40
static constexpr int TILES_Y = SCREEN_H / TILE_H;   // 28

// VRAM layout constants (Sonic 2 uses these addresses)
static constexpr int VRAM_SIZE  = 0x10000;
static constexpr int CRAM_SIZE  = 64;    // 4 palettes × 16 colours
static constexpr int VSRAM_SIZE = 40;

// Nametable entry flags
static constexpr uint16_t NT_PRIORITY = 0x8000;
static constexpr uint16_t NT_PALETTE  = 0x6000;
static constexpr uint16_t NT_VFLIP    = 0x1000;
static constexpr uint16_t NT_HFLIP    = 0x0800;
static constexpr uint16_t NT_TILE     = 0x07FF;

// Sprite attribute table entry (8 bytes in VRAM)
struct SpriteAttr {
    uint16_t y;       // Y position (offset by 128)
    uint8_t  size;    // H/V size in tiles (bits [3:2] = H, [1:0] = V, each 0–3 → 1–4 tiles)
    uint8_t  link;    // next sprite index
    uint16_t attr;    // priority | palette | vflip | hflip | tile_index
    uint16_t x;       // X position (offset by 128)
};

class VDP {
public:
    VDP();
    ~VDP();

    bool init(SDL_Renderer *renderer);
    void shutdown();

    // Direct VRAM/CRAM/VSRAM access (used by translated game code)
    uint8_t  *vram()  { return m_vram.data(); }
    uint16_t *cram()  { return m_cram.data(); }
    uint16_t *vsram() { return m_vsram.data(); }

    // Register writes (translated from move.w #val, (VDP_ctrl) patterns)
    void write_reg(int reg, uint8_t val) { m_regs[reg] = val; }
    uint8_t read_reg(int reg) const { return m_regs[reg]; }

    // Palette helper: write one Genesis 9-bit colour to CRAM slot
    // slot = palette*16 + index
    void set_colour(int slot, uint16_t genesis_colour);

    // DMA fill/copy shortcuts used heavily in Sonic 2 init routines
    void dma_fill(uint16_t vram_addr, uint8_t value, uint16_t len);
    void dma_copy(uint16_t dst, uint16_t src, uint16_t len);

    // High-level tile write: writes a 4bpp tile (32 bytes) at the given index
    void write_tile(int tile_index, const uint8_t *data);

    // Write a nametable row (for level plane updates)
    void write_nametable(int plane, int tile_col, int tile_row, uint16_t entry);

    // Present one frame: renders planes + sprites to the SDL texture, then blits
    void present_frame(int plane_a_hscroll, int plane_b_hscroll,
                       int plane_a_vscroll, int plane_b_vscroll);

    // Nametable base addresses (derived from VDP registers, same formula as hardware)
    uint32_t plane_a_base() const { return (uint32_t)(m_regs[2] & 0x38) << 10; }
    uint32_t plane_b_base() const { return (uint32_t)(m_regs[4] & 0x07) << 13; }
    uint32_t sprite_table_base() const { return (uint32_t)(m_regs[5] & 0x7F) << 9; }

    // Plane dimensions in tiles (from reg 16)
    int plane_w_tiles() const;
    int plane_h_tiles() const;

private:
    std::array<uint8_t,  VRAM_SIZE>  m_vram{};
    std::array<uint16_t, CRAM_SIZE>  m_cram{};
    std::array<uint16_t, VSRAM_SIZE> m_vsram{};
    std::array<uint8_t,  24>         m_regs{};

    SDL_Renderer *m_renderer = nullptr;
    SDL_Texture  *m_texture  = nullptr;

    // Per-frame render helpers
    void render_plane(uint32_t *pixels, int line, uint32_t nt_base,
                      int hscroll, int vscroll, bool high_pri);
    void render_sprites(uint32_t *pixels, int line, bool high_pri);

    uint32_t cram_to_argb(uint16_t c) const;
    uint8_t  tile_pixel(uint16_t tile_idx, int row, int col) const;
};
