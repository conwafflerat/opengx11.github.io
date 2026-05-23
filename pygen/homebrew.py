"""A homebrew test ROM built in-process.

It mirrors what a real game's boot code does -- initialise the VDP registers,
then DMA a palette into CRAM, tile art into VRAM and a tilemap into plane A,
then enable display and idle. This exercises the CPU -> bus -> VDP DMA path
that booting Sonic depends on, without needing a copyrighted ROM.
"""

from __future__ import annotations

import struct

# Fixed ROM layout.
PROG = 0x0200
STUB = 0x0500
PALETTE = 0x2000
TILES = 0x2100
NAMETABLE = 0x3000
ROM_SIZE = 0x4000

VDP_CTRL = 0x00C00004

# Standard register init (reg, value). reg1 = 0x74: display on, VINT, DMA, mode5.
VDP_INIT = [
    (0x00, 0x04), (0x01, 0x74), (0x02, 0x30), (0x03, 0x00),
    (0x04, 0x07), (0x05, 0x78), (0x06, 0x00), (0x07, 0x00),
    (0x0A, 0xFF), (0x0B, 0x00), (0x0C, 0x81), (0x0D, 0x3F),
    (0x0F, 0x02), (0x10, 0x01), (0x11, 0x00), (0x12, 0x00),
]


class _Asm:
    def __init__(self):
        self.words = []

    def w(self, *words):
        self.words.extend(words)

    def move_w_abs(self, imm, addr):           # MOVE.W #imm,(addr).L
        self.w(0x33FC, imm & 0xFFFF, (addr >> 16) & 0xFFFF, addr & 0xFFFF)

    def vdp_reg(self, reg, val):
        self.move_w_abs(0x8000 | (reg << 8) | (val & 0xFF), VDP_CTRL)

    def vdp_ctrl(self, word):
        self.move_w_abs(word & 0xFFFF, VDP_CTRL)

    def move_w_sr(self, imm):                  # MOVE.W #imm,SR
        self.w(0x46FC, imm & 0xFFFF)

    def bra_self(self):                        # BRA *  (tight idle loop)
        self.w(0x60FE)

    def dma(self, first, second, src, length_words):
        s = src >> 1
        self.vdp_reg(0x13, length_words & 0xFF)
        self.vdp_reg(0x14, (length_words >> 8) & 0xFF)
        self.vdp_reg(0x15, s & 0xFF)
        self.vdp_reg(0x16, (s >> 8) & 0xFF)
        self.vdp_reg(0x17, (s >> 16) & 0x7F)   # bit7 = 0 -> 68k memory to VDP
        self.vdp_ctrl(first)
        self.vdp_ctrl(second)


def _vram_cmd(dest):
    return 0x4000 | (dest & 0x3FFF), 0x80 | ((dest >> 14) & 3)


def _cram_cmd(dest):
    return 0xC000 | (dest & 0x3FFF), 0x80 | ((dest >> 14) & 3)


def build_rom() -> bytes:
    rom = bytearray(ROM_SIZE)

    # Vectors: SP, reset PC, then point every exception/IRQ vector at an RTE.
    struct.pack_into(">I", rom, 0x00, 0x00FFE000)
    struct.pack_into(">I", rom, 0x04, PROG)
    for vec in range(2, 64):
        struct.pack_into(">I", rom, vec * 4, STUB)
    struct.pack_into(">H", rom, STUB, 0x4E73)   # RTE

    rom[0x100:0x110] = b"SEGA MEGA DRIVE "
    rom[0x120:0x140] = b"PYGEN HOMEBREW DMA TEST".ljust(0x20)

    # Program.
    asm = _Asm()
    asm.move_w_sr(0x2700)                        # interrupts off during setup
    for reg, val in VDP_INIT:
        asm.vdp_reg(reg, val)
    f, s = _cram_cmd(0x00)
    asm.dma(f, s, PALETTE, 16)                   # palette -> CRAM
    f, s = _vram_cmd(0x20)
    asm.dma(f, s, TILES, 16 * 16)               # 16 tiles -> VRAM (tile 1..16)
    f, s = _vram_cmd(0xC000)
    asm.dma(f, s, NAMETABLE, 64 * 32)           # tilemap -> plane A
    asm.bra_self()
    for i, word in enumerate(asm.words):
        struct.pack_into(">H", rom, PROG + i * 2, word)

    # Palette: a 16-entry rainbow (Genesis ----BBB-GGG-RRR-).
    for i in range(16):
        r = (i * 7) // 15
        g = ((15 - i) * 7) // 15
        b = (i * 3) % 8
        color = ((b & 7) << 9) | ((g & 7) << 5) | ((r & 7) << 1)
        struct.pack_into(">H", rom, PALETTE + i * 2, color)

    # Tiles 1..15: solid blocks of colour index n (32 bytes each).
    for n in range(1, 16):
        base = TILES + n * 32
        fill = (n << 4) | n
        for j in range(32):
            rom[base + j] = fill

    # Plane A nametable: a diagonal banding pattern (64x32 entries).
    for row in range(32):
        for col in range(64):
            tile = ((col + row) % 15) + 1
            struct.pack_into(">H", rom, NAMETABLE + (row * 64 + col) * 2, tile)

    return bytes(rom)


def run_homebrew(frames: int = 2, out: str = "homebrew.ppm") -> int:
    from .emulator import Genesis
    from .rom import Rom
    from .display import save_ppm

    genesis = Genesis(Rom(build_rom()))
    for _ in range(frames):
        genesis.run_frame()
    width, height, rgb = genesis.render()
    bg = (rgb[0], rgb[1], rgb[2])
    non_bg = sum(1 for i in range(0, len(rgb), 3)
                 if (rgb[i], rgb[i + 1], rgb[i + 2]) != bg)
    save_ppm(out, width, height, rgb)
    print(f"Ran homebrew ROM for {frames} frame(s); wrote {out} ({width}x{height})")
    print(f"Instructions executed: {genesis.cpu.instructions}")
    print(f"Non-background pixels: {non_bg} / {width * height}")
    if genesis.cpu.illegal_log:
        print("Unimplemented opcodes:",
              {f"0x{op:04X}": n for op, n in genesis.cpu.illegal_log.items()})
    return 0
