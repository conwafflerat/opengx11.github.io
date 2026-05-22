#include "vdp.hpp"
#include <cstring>
#include <algorithm>

VDP::VDP()  = default;
VDP::~VDP() { shutdown(); }

bool VDP::init(SDL_Renderer *renderer)
{
    m_renderer = renderer;
    m_texture  = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H);
    return m_texture != nullptr;
}

void VDP::shutdown()
{
    if (m_texture) { SDL_DestroyTexture(m_texture); m_texture = nullptr; }
}

// Genesis 9-bit BGR → ARGB8888
uint32_t VDP::cram_to_argb(uint16_t c) const
{
    uint8_t r = ((c >> 1) & 7) * 36;
    uint8_t g = ((c >> 5) & 7) * 36;
    uint8_t b = ((c >> 9) & 7) * 36;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint8_t VDP::tile_pixel(uint16_t idx, int row, int col) const
{
    uint32_t off = (uint32_t)idx * 32 + (uint32_t)row * 4 + (col >> 1);
    uint8_t  b   = m_vram[off & 0xFFFF];
    return (col & 1) ? (b & 0x0F) : (b >> 4);
}

void VDP::set_colour(int slot, uint16_t genesis_colour)
{
    if (slot >= 0 && slot < CRAM_SIZE)
        m_cram[slot] = genesis_colour;
}

void VDP::dma_fill(uint16_t addr, uint8_t value, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        m_vram[(addr + i) & 0xFFFF] = value;
}

void VDP::dma_copy(uint16_t dst, uint16_t src, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        m_vram[(dst + i) & 0xFFFF] = m_vram[(src + i) & 0xFFFF];
}

void VDP::write_tile(int tile_index, const uint8_t *data)
{
    uint32_t off = (uint32_t)tile_index * 32;
    std::memcpy(m_vram.data() + (off & 0xFFFF), data, 32);
}

void VDP::write_nametable(int plane, int tc, int tr, uint16_t entry)
{
    uint32_t base = (plane == 0) ? plane_a_base() : plane_b_base();
    int pw = plane_w_tiles();
    uint32_t off = (base + (uint32_t)(tr * pw + tc) * 2) & 0xFFFF;
    m_vram[off]     = entry >> 8;
    m_vram[off + 1] = entry & 0xFF;
}

int VDP::plane_w_tiles() const
{
    static const int t[] = {32, 64, 0, 128};
    return t[m_regs[16] & 3];
}

int VDP::plane_h_tiles() const
{
    static const int t[] = {32, 64, 0, 128};
    return t[(m_regs[16] >> 4) & 3];
}

// ---- Rendering -------------------------------------------------------

void VDP::render_plane(uint32_t *pixels, int line, uint32_t nt_base,
                       int hscroll, int vscroll, bool high_pri)
{
    int pw = plane_w_tiles(), ph = plane_h_tiles();
    if (!pw || !ph) return;

    int pw_px = pw * TILE_W, ph_px = ph * TILE_H;
    int eff_y = (line + vscroll) & (ph_px - 1);
    int trow  = eff_y / TILE_H, prow = eff_y % TILE_H;

    for (int x = 0; x < SCREEN_W; x++) {
        int eff_x = (x - hscroll) & (pw_px - 1);
        int tcol  = eff_x / TILE_W, pcol = eff_x % TILE_W;

        uint32_t off   = (nt_base + (uint32_t)(trow * pw + tcol) * 2) & 0xFFFF;
        uint16_t entry = (uint16_t)(m_vram[off] << 8) | m_vram[off + 1];

        bool     pri   = (entry & NT_PRIORITY) != 0;
        if (pri != high_pri) continue;

        uint8_t  pal   = (entry & NT_PALETTE) >> 13;
        bool     vflip = (entry & NT_VFLIP) != 0;
        bool     hflip = (entry & NT_HFLIP) != 0;
        uint16_t tidx  = entry & NT_TILE;

        int pr = vflip ? (TILE_H - 1 - prow) : prow;
        int pc = hflip ? (TILE_W - 1 - pcol) : pcol;

        uint8_t ci = tile_pixel(tidx, pr, pc);
        if (!ci) continue;

        pixels[x] = cram_to_argb(m_cram[pal * 16 + ci]);
    }
}

void VDP::render_sprites(uint32_t *pixels, int line, bool high_pri)
{
    uint32_t sat = sprite_table_base();
    int count = 0, link = 0;

    for (int s = 0; s < 80; s++) {
        uint32_t off = (sat + (uint32_t)link * 8) & 0xFFFF;

        int16_t  y    = (int16_t)(((m_vram[off] & 3) << 8) | m_vram[off + 1]) - 128;
        uint8_t  sz   = m_vram[off + 2];
        uint8_t  vs   = (sz >> 2 & 3) + 1;
        uint8_t  hs   = (sz & 3) + 1;
        link          = m_vram[off + 3] & 0x7F;

        uint16_t attr  = (uint16_t)(m_vram[off + 4] << 8) | m_vram[off + 5];
        int16_t  x     = (int16_t)(((m_vram[off + 6] & 3) << 8) | m_vram[off + 7]) - 128;

        if (line < y || line >= y + vs * TILE_H) goto next;
        if (((attr & 0x8000) != 0) != high_pri) goto next;
        if (++count > 20) break;

        {
            bool    vflip = (attr & 0x1000) != 0;
            bool    hflip = (attr & 0x0800) != 0;
            uint8_t pal   = (attr >> 13) & 3;
            uint16_t tidx = attr & 0x7FF;

            int srow = line - y;
            if (vflip) srow = vs * TILE_H - 1 - srow;
            int trow_s = srow / TILE_H, prow = srow % TILE_H;

            for (int col = 0; col < hs * TILE_W; col++) {
                int px = x + col;
                if (px < 0 || px >= SCREEN_W) continue;

                int pc2  = hflip ? (hs * TILE_W - 1 - col) : col;
                int tcol_s = pc2 / TILE_W, pcol = pc2 % TILE_W;
                uint16_t t = tidx + (uint16_t)(tcol_s * vs + trow_s);

                uint8_t ci = tile_pixel(t, prow, pcol);
                if (!ci) continue;
                pixels[px] = cram_to_argb(m_cram[pal * 16 + ci]);
            }
        }
next:
        if (!link) break;
    }
}

void VDP::present_frame(int hs_a, int hs_b, int vs_a, int vs_b)
{
    void   *raw;
    int     pitch;
    SDL_LockTexture(m_texture, nullptr, &raw, &pitch);
    uint32_t *pixels = reinterpret_cast<uint32_t *>(raw);
    int stride = pitch / 4;

    // Background colour from reg 7
    uint8_t  bg_pal = (m_regs[7] >> 4) & 3;
    uint8_t  bg_idx =  m_regs[7] & 0xF;
    uint32_t bg     = cram_to_argb(m_cram[bg_pal * 16 + bg_idx]);

    bool display_on = (m_regs[1] & 0x40) != 0;

    uint32_t nt_a = plane_a_base();
    uint32_t nt_b = plane_b_base();

    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t *row = pixels + y * stride;
        std::fill(row, row + SCREEN_W, bg);
        if (!display_on) continue;

        // Low priority: Plane B, Plane A
        render_plane(row, y, nt_b, hs_b, vs_b, false);
        render_plane(row, y, nt_a, hs_a, vs_a, false);
        render_sprites(row, y, false);

        // High priority: Plane B, Plane A, sprites
        render_plane(row, y, nt_b, hs_b, vs_b, true);
        render_plane(row, y, nt_a, hs_a, vs_a, true);
        render_sprites(row, y, true);
    }

    SDL_UnlockTexture(m_texture);

    // Blit to renderer with aspect-correct scaling
    int ww, wh;
    SDL_GetRendererOutputSize(m_renderer, &ww, &wh);
    float aspect = (float)SCREEN_W / SCREEN_H;
    SDL_Rect dst{0, 0, ww, wh};
    if ((float)ww / wh > aspect) {
        dst.w = (int)(wh * aspect);
        dst.x = (ww - dst.w) / 2;
    } else {
        dst.h = (int)(ww / aspect);
        dst.y = (wh - dst.h) / 2;
    }
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, &dst);
    SDL_RenderPresent(m_renderer);
}
