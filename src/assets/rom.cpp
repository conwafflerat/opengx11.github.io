#include "rom.hpp"
#include <fstream>
#include <cstdio>

Rom g_rom;

bool Rom::load(const std::string &path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        fprintf(stderr, "Cannot open ROM: %s\n", path.c_str());
        return false;
    }
    auto sz = f.tellg();
    if (sz <= 0 || sz > 4 * 1024 * 1024) {
        fprintf(stderr, "ROM size out of range: %lld bytes\n", (long long)sz);
        return false;
    }
    m_data.resize((size_t)sz);
    f.seekg(0);
    f.read(reinterpret_cast<char *>(m_data.data()), sz);
    printf("ROM loaded: %s (%zu KB)\n", path.c_str(), m_data.size() / 1024);
    return true;
}

uint8_t Rom::u8(uint32_t o) const
{
    if (o >= m_data.size()) return 0xFF;
    return m_data[o];
}

uint16_t Rom::u16(uint32_t o) const
{
    if (o + 1 >= m_data.size()) return 0xFFFF;
    return (uint16_t)(m_data[o] << 8) | m_data[o + 1];
}

uint32_t Rom::u32(uint32_t o) const
{
    return ((uint32_t)u16(o) << 16) | u16(o + 2);
}

const uint8_t *Rom::ptr(uint32_t o) const
{
    if (o >= m_data.size()) return nullptr;
    return m_data.data() + o;
}

const uint16_t *Rom::zone_palette(int zone, int act) const
{
    // Each zone/act entry in the palette table is 64 words (128 bytes)
    uint32_t off = PaletteTable + (uint32_t)(zone * 2 + act) * 128;
    if (off + 128 > m_data.size()) return nullptr;
    return reinterpret_cast<const uint16_t *>(m_data.data() + off);
}
