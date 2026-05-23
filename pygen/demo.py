"""A built-in VDP demo scene (no ROM required).

Programs the VDP the way a game's setup code would -- a palette, a set of
solid-color tiles and a plane-A nametable plus one sprite -- then renders a
frame. Useful for confirming the renderer works end to end and for producing a
sample image.
"""

from __future__ import annotations

import struct

from .vdp import VDP


def _color(r: int, g: int, b: int) -> int:
    """Pack 3-bit R/G/B into a Genesis CRAM word (----BBB-GGG-RRR-)."""
    return ((b & 7) << 9) | ((g & 7) << 5) | ((r & 7) << 1)


def build_demo_vdp() -> VDP:
    vdp = VDP()
    vdp.regs[1] = 0x44      # display enabled, mode 5
    vdp.regs[12] = 0x81     # H40 (320 px wide)
    vdp.regs[16] = 0x01     # scroll plane 64x32 cells
    vdp.regs[2] = 0x30      # plane A nametable base -> 0xC000
    vdp.regs[4] = 0x07      # plane B nametable base -> 0xE000
    vdp.regs[5] = 0x78      # sprite attribute table -> 0xF000
    vdp.regs[15] = 0x02     # auto-increment 2 bytes
    vdp.regs[7] = 0x00      # background = CRAM[0]

    # Palette 0: a rainbow gradient across the 16 entries.
    vdp.cram[0] = _color(0, 0, 1)
    for i in range(1, 16):
        r = (i * 7) // 15
        g = ((15 - i) * 7) // 15
        b = ((i * 3) % 8)
        vdp.cram[i] = _color(r, g, b)

    # Tiles 1..15: each a solid block of its own color index.
    for n in range(1, 16):
        base = n * 32
        fill = (n << 4) | n
        for j in range(32):
            vdp.vram[base + j] = fill

    # Plane A nametable at 0xC000: a diagonal banding pattern.
    base = 0xC000
    for row in range(32):
        for col in range(64):
            tile = ((col + row) % 15) + 1
            entry = tile  # palette 0, no flip, low priority
            off = base + (row * 64 + col) * 2
            struct.pack_into(">H", vdp.vram, off, entry)

    # One 16x16 sprite (2x2 tiles) near the centre using tiles 4..7.
    sprite = 0xF000
    struct.pack_into(">H", vdp.vram, sprite + 0, 128 + 100)   # Y
    vdp.vram[sprite + 2] = 0b0101                              # size 2x2
    vdp.vram[sprite + 3] = 0                                   # link = end
    struct.pack_into(">H", vdp.vram, sprite + 4, 0x0004)      # tile 4, pal 0
    struct.pack_into(">H", vdp.vram, sprite + 6, 128 + 150)   # X

    return vdp


def run_demo(out: str = "demo.ppm") -> int:
    from .display import save_ppm
    vdp = build_demo_vdp()
    width, height, rgb = vdp.render()
    non_bg = sum(1 for i in range(0, len(rgb), 3)
                 if (rgb[i], rgb[i + 1], rgb[i + 2]) != (rgb[0], rgb[1], rgb[2]))
    save_ppm(out, width, height, rgb)
    print(f"Rendered {width}x{height} demo frame -> {out}")
    print(f"Non-background pixels: {non_bg} / {width * height}")
    return 0
