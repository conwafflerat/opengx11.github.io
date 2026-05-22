#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// Kosinski decompressor — the format used for all art in Sonic 1/2/CD.
//
// Algorithm (Sonic Retro wiki):
//   16-bit LE flag word consumed LSB-first; replenished when exhausted.
//   Flag bit 1 → literal byte copy.
//   Flag bit 0 → dictionary lookback copy:
//     2-byte near form (B2 & 0xE0 != 0): 13-bit negative offset, 3–10-byte count
//     3-byte far  form (B2 & 0xE0 == 0): 13-bit negative offset, 1–256-byte count
//   Terminator: 3-byte form with B1==0 AND B2==0, or count byte == 0 in 2-byte form.

// Decompress a Kosinski stream starting at src into dst.
// Returns number of bytes written, or 0 on error.
size_t kosinski_decomp(const uint8_t *src, size_t src_len,
                       std::vector<uint8_t> &dst);
