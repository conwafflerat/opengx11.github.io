"""A simple Genesis VDP (Video Display Processor).

Implements enough to drive the typical setup path of a game: register writes,
VRAM/CRAM/VSRAM access through the data port with address auto-increment, the
status register, the vertical-interrupt flag, and a tile-based renderer for
plane A, plane B and sprites.

Simplifications: scrolling is whole-screen only (no per-line/per-cell scroll),
the window plane and shadow/highlight modes are ignored, and DMA is handled as
an immediate memory fill/copy rather than being timed.
"""

from __future__ import annotations

# VDP control-port command codes (CD5..CD0).
CD_VRAM_WRITE = 0x01
CD_CRAM_WRITE = 0x03
CD_VSRAM_WRITE = 0x05
CD_VRAM_READ = 0x00
CD_CRAM_READ = 0x08
CD_VSRAM_READ = 0x04
CD_DMA = 0x20

SCREEN_H = 224


class VDP:
    def __init__(self):
        self.vram = bytearray(0x10000)
        self.cram = [0] * 64        # 9-bit color entries
        self.vsram = [0] * 40
        self.regs = [0] * 32

        self.ctrl_pending = False   # waiting for the 2nd control word
        self.ctrl_first = 0
        self.address = 0
        self.code = 0

        self.status = 0x3400        # FIFO empty etc.
        self.vint_pending = False

    # --------------------------------------------------------------- ports
    def read_port(self, offset: int) -> int:
        offset &= 0x1E
        if offset < 0x04:           # data port
            return self._data_read()
        if offset < 0x08:           # control port -> status
            self.ctrl_pending = False
            return self.status
        return 0

    def write_port(self, offset: int, value: int):
        offset &= 0x1E
        value &= 0xFFFF
        if offset < 0x04:           # data port
            self._data_write(value)
        elif offset < 0x08:         # control port
            self._control_write(value)

    # ------------------------------------------------------------- control
    def _control_write(self, value: int):
        if not self.ctrl_pending:
            if (value & 0xC000) == 0x8000:     # register write
                reg = (value >> 8) & 0x1F
                self.regs[reg] = value & 0xFF
                return
            self.ctrl_first = value
            self.ctrl_pending = True
            self.code = (value >> 14) & 0x03
            self.address = value & 0x3FFF
        else:
            self.ctrl_pending = False
            self.code |= ((value >> 4) & 0x0F) << 2
            self.address |= (value & 0x03) << 14
            if self.code & CD_DMA and (self.regs[1] & 0x10):
                self._do_dma()

    @property
    def auto_inc(self) -> int:
        return self.regs[15]

    # ---------------------------------------------------------------- data
    def _data_write(self, value: int):
        cd = self.code & 0x0F
        addr = self.address
        if cd == CD_CRAM_WRITE:
            self.cram[(addr >> 1) & 0x3F] = value & 0x0FFF
        elif cd == CD_VSRAM_WRITE:
            self.vsram[(addr >> 1) % 40] = value & 0x07FF
        else:                       # default to VRAM
            a = addr & 0xFFFF
            self.vram[a] = (value >> 8) & 0xFF
            self.vram[(a + 1) & 0xFFFF] = value & 0xFF
        self.address = (self.address + self.auto_inc) & 0xFFFF

    def _data_read(self) -> int:
        cd = self.code & 0x0F
        addr = self.address
        if cd == CD_CRAM_READ:
            value = self.cram[(addr >> 1) & 0x3F]
        elif cd == CD_VSRAM_READ:
            value = self.vsram[(addr >> 1) % 40]
        else:
            a = addr & 0xFFFF
            value = (self.vram[a] << 8) | self.vram[(a + 1) & 0xFFFF]
        self.address = (self.address + self.auto_inc) & 0xFFFF
        return value

    def _do_dma(self):
        # Only the simplest case: 68000 memory -> VRAM is handled by the bus
        # writing through the data port, so here we cover VRAM fill (mode 2).
        length = (self.regs[19] | (self.regs[20] << 8)) or 0x10000
        dma_type = (self.regs[23] >> 6) & 3
        if dma_type == 2:           # VRAM fill
            fill = self.vram[self.address & 0xFFFF]
            for _ in range(length):
                self.vram[self.address & 0xFFFF] = fill
                self.address = (self.address + self.auto_inc) & 0xFFFF

    # ------------------------------------------------------------- timing
    def start_vblank(self):
        self.status |= 0x0008       # vertical blanking flag
        if self.regs[1] & 0x20:     # VINT enabled
            self.vint_pending = True

    def end_vblank(self):
        self.status &= ~0x0008

    @property
    def display_enabled(self) -> bool:
        return bool(self.regs[1] & 0x40)

    @property
    def screen_width(self) -> int:
        return 320 if (self.regs[12] & 0x81) else 256

    # ----------------------------------------------------------- rendering
    def _plane_dims(self):
        sizes = {0: 32, 1: 64, 0b11: 128}
        w = sizes.get(self.regs[16] & 0x03, 32)
        h = sizes.get((self.regs[16] >> 4) & 0x03, 32)
        return w, h

    @staticmethod
    def _color_to_rgb(c: int):
        # Genesis CRAM: ----BBB-GGG-RRR- (3 bits each, scaled to 8 bits).
        r = (c >> 1) & 0x7
        g = (c >> 5) & 0x7
        b = (c >> 9) & 0x7
        scale = (0, 36, 73, 109, 146, 182, 219, 255)
        return scale[r], scale[g], scale[b]

    def _draw_tile(self, fb, sx, sy, tile_index, palette, hflip, vflip, width):
        base = (tile_index & 0x7FF) * 32
        for ty in range(8):
            py = sy + (7 - ty if vflip else ty)
            if py < 0 or py >= SCREEN_H:
                continue
            row = base + ty * 4
            for tx in range(8):
                px = sx + (7 - tx if hflip else tx)
                if px < 0 or px >= width:
                    continue
                byte = self.vram[(row + (tx >> 1)) & 0xFFFF]
                pix = (byte >> 4) if (tx & 1) == 0 else (byte & 0x0F)
                if pix == 0:
                    continue        # transparent
                color = self.cram[(palette * 16 + pix) & 0x3F]
                r, g, b = self._color_to_rgb(color)
                off = (py * width + px) * 3
                fb[off] = r
                fb[off + 1] = g
                fb[off + 2] = b

    def _draw_plane(self, fb, base_addr, width, scroll_x, scroll_y):
        pw, ph = self._plane_dims()
        cols = width // 8
        rows = SCREEN_H // 8
        for row in range(rows + 1):
            for col in range(cols + 1):
                map_col = ((col + (scroll_x // 8)) % pw)
                map_row = ((row + (scroll_y // 8)) % ph)
                entry_addr = base_addr + (map_row * pw + map_col) * 2
                entry = (self.vram[entry_addr & 0xFFFF] << 8) | self.vram[(entry_addr + 1) & 0xFFFF]
                tile = entry & 0x7FF
                hflip = bool(entry & 0x0800)
                vflip = bool(entry & 0x1000)
                palette = (entry >> 13) & 0x3
                sx = col * 8 - (scroll_x % 8)
                sy = row * 8 - (scroll_y % 8)
                self._draw_tile(fb, sx, sy, tile, palette, hflip, vflip, width)

    def _draw_sprites(self, fb, width):
        table = (self.regs[5] & 0x7F) << 9
        link = 0
        for _ in range(80):
            base = (table + link * 8) & 0xFFFF
            y = ((self.vram[base] << 8) | self.vram[base + 1]) & 0x3FF
            size = self.vram[base + 2]
            link_next = self.vram[base + 3] & 0x7F
            attr = (self.vram[base + 4] << 8) | self.vram[base + 5]
            x = ((self.vram[base + 6] << 8) | self.vram[base + 7]) & 0x1FF

            w_tiles = ((size >> 2) & 3) + 1
            h_tiles = (size & 3) + 1
            tile = attr & 0x7FF
            hflip = bool(attr & 0x0800)
            vflip = bool(attr & 0x1000)
            palette = (attr >> 13) & 3

            sx0 = x - 128
            sy0 = y - 128
            for cx in range(w_tiles):
                for cy in range(h_tiles):
                    t = tile + (cx * h_tiles + cy)
                    tx = (w_tiles - 1 - cx) if hflip else cx
                    ty = (h_tiles - 1 - cy) if vflip else cy
                    self._draw_tile(fb, sx0 + tx * 8, sy0 + ty * 8,
                                    t, palette, hflip, vflip, width)
            if link_next == 0:
                break
            link = link_next

    def render(self) -> tuple:
        """Render one frame; returns (width, height, RGB bytearray)."""
        width = self.screen_width
        fb = bytearray(width * SCREEN_H * 3)

        # Background color.
        bg = self.cram[(self.regs[7] & 0x3F)]
        r, g, b = self._color_to_rgb(bg)
        for i in range(0, len(fb), 3):
            fb[i] = r
            fb[i + 1] = g
            fb[i + 2] = b

        if not self.display_enabled:
            return width, SCREEN_H, fb

        scroll_x = (self.vram[((self.regs[13] & 0x3F) << 10) + 1]
                    | (self.vram[(self.regs[13] & 0x3F) << 10] << 8))
        scroll_y = self.vsram[0]

        plane_b = (self.regs[4] & 0x07) << 13
        plane_a = (self.regs[2] & 0x38) << 10
        self._draw_plane(fb, plane_b, width, -scroll_x & 0x3FF, scroll_y)
        self._draw_plane(fb, plane_a, width, -scroll_x & 0x3FF, scroll_y)
        self._draw_sprites(fb, width)
        return width, SCREEN_H, fb
