#include "kosinski.hpp"

size_t kosinski_decomp(const uint8_t *src, size_t src_len,
                       std::vector<uint8_t> &dst)
{
    dst.clear();
    dst.reserve(0x8000);  // typical art chunk is ~32 KB uncompressed

    if (src_len < 2) return 0;

    const uint8_t *in     = src;
    const uint8_t *in_end = src + src_len;

    auto read_byte = [&]() -> uint8_t {
        if (in >= in_end) return 0;
        return *in++;
    };

    // Initial 16-bit little-endian flag word
    uint16_t flags = read_byte() | ((uint16_t)read_byte() << 8);
    int      bits  = 16;

    for (;;) {
        // Replenish flag word when exhausted
        if (bits == 0) {
            flags = read_byte() | ((uint16_t)read_byte() << 8);
            bits  = 16;
        }

        bool lit = flags & 1;
        flags >>= 1;
        --bits;

        if (lit) {
            // ---- Literal: copy one byte from compressed stream ----
            dst.push_back(read_byte());
        } else {
            // ---- Dictionary lookback copy ----
            uint8_t b1 = read_byte();
            uint8_t b2 = read_byte();

            int     count;
            int16_t offset;

            if (b2 & 0xE0) {
                // 2-byte near form: offset uses 13 bits, count 3 bits
                count = b2 & 7;
                if (count == 0) {
                    count = (int)read_byte();
                    if (count == 0) break;  // terminator
                    ++count;
                } else {
                    count += 2;
                }
                // Negative 13-bit offset sign-extended to 16 bit:
                //   upper 5 bits come from b2[7:3], lower 8 from b1
                offset = (int16_t)(0xE000u | (uint16_t)b1 |
                                   ((uint16_t)(b2 & 0xF8) << 5));
            } else {
                // 3-byte far form
                if (b1 == 0 && b2 == 0) break;  // terminator
                count  = (int)read_byte() + 1;
                offset = (int16_t)(0xE000u | (uint16_t)b1 |
                                   ((uint16_t)b2 << 8));
            }

            // Copy 'count' bytes from current-output-pos + offset (negative)
            size_t base = dst.size();
            for (int i = 0; i < count; ++i) {
                size_t src_idx = (size_t)((ptrdiff_t)base + (ptrdiff_t)i + offset);
                dst.push_back(src_idx < dst.size() ? dst[src_idx] : 0);
            }
        }
    }

    return dst.size();
}
