#pragma once
#include <cstdint>
#include <vector>
#include <string>

// ============================================================
// ROM accessor — loads the Sonic 2 ROM and exposes the original
// data (art, level layouts, palettes, etc.) as typed pointers.
//
// All ROM offsets are documented in s2disasm/s2.asm.
// ============================================================

class Rom {
public:
    bool load(const std::string &path);
    bool loaded() const { return !m_data.empty(); }

    const uint8_t *data() const { return m_data.data(); }
    size_t         size() const { return m_data.size(); }

    // Safe bounds-checked read helpers
    uint8_t  u8 (uint32_t offset) const;
    uint16_t u16(uint32_t offset) const;  // big-endian
    uint32_t u32(uint32_t offset) const;  // big-endian
    const uint8_t *ptr(uint32_t offset) const;

    // ---- Known ROM offsets (from s2disasm) ------------------
    // Art pointers table: each entry is a 32-bit offset to compressed art
    static constexpr uint32_t ArtPtrs_Table  = 0x000014;  // adjust per actual ROM
    // Level layout (chunks): zone/act indexed
    static constexpr uint32_t LevelLayouts   = 0x0003A0;
    // 128x128 block mappings per zone
    static constexpr uint32_t ChunkTable     = 0x000450;
    // Palette table
    static constexpr uint32_t PaletteTable   = 0x002782;
    // Object placement list
    static constexpr uint32_t ObjPosTable    = 0x000694;
    // Ring position list
    static constexpr uint32_t RingPosTable   = 0x000696;
    // Collision index arrays
    static constexpr uint32_t ColIndexA      = 0x038000;
    static constexpr uint32_t ColIndexB      = 0x039000;
    // Collision height arrays
    static constexpr uint32_t ColHeightArray = 0x03A000;
    // Collision angle arrays
    static constexpr uint32_t ColAngleArray  = 0x03C000;

    // Palette for a given zone+act (returns pointer to 64 words = 4 palettes × 16 colours)
    const uint16_t *zone_palette(int zone, int act) const;

private:
    std::vector<uint8_t> m_data;
};

// Global ROM instance (set up in main before anything else)
extern Rom g_rom;
