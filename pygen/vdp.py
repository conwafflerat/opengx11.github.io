"""A simple Genesis VDP (Video Display Processor).

Covers the parts a game's setup and per-frame code actually use:

  * register writes and VRAM/CRAM/VSRAM access via the data/control ports,
  * the three DMA modes (68k memory -> VDP, VRAM fill, VRAM copy),
  * the status register, vblank flag and an approximate H/V counter,
  * a scanline renderer with per-line horizontal scroll, full-screen vertical
    scroll, plane A/B, sprites and a back-to-front priority model.

Simplifications: per-column vertical scroll, the window plane and
shadow/highlight modes are not implemented, and DMA happens instantly rather
than being timed against the raster.
"""

from __future__ import annotations

# Control-port command codes (low bits; the DMA bit is 0x20).
CD_DMA = 0x20

SCREEN_H = 224

# 3-bit colour component -> 8-bit, matching real Genesis output levels.
_LEVELS = (0, 36, 73, 109, 146, 182, 219, 255)


class VDP:
    def __init__(self, bus=None):
        self.bus = bus
        self.vram = bytearray(0x10000)
        self.cram = [0] * 64        # 9-bit colour entries
        self.vsram = [0] * 40
        self.regs = [0] * 32

        self.ctrl_pending = False
        self.address = 0
        self.code = 0

        self.status = 0x3400
        self.vint_pending = False
        self.dma_fill_pending = False
        self.current_line = 0

    # --------------------------------------------------------------- ports
    def read_port(self, offset: int) -> int:
        offset &= 0x1E
        if offset < 0x04:
            return self._data_read()
        if offset < 0x08:
            self.ctrl_pending = False
            return self.status
        return self.read_hv()

    def write_port(self, offset: int, value: int):
        offset &= 0x1E
        value &= 0xFFFF
        if offset < 0x04:
            self._data_write(value)
        elif offset < 0x08:
            self._control_write(value)

    def read_hv(self) -> int:
        line = self.current_line
        v = line if line <= 0xEA else line - 6   # rough NTSC v-counter
        return ((v & 0xFF) << 8) | 0x00

    # ------------------------------------------------------------- control
    def _control_write(self, value: int):
        if not self.ctrl_pending:
            if (value & 0xC000) == 0x8000:     # register write
                reg = (value >> 8) & 0x1F
                self.regs[reg] = value & 0xFF
                return
            self.code = (value >> 14) & 0x03
            self.address = value & 0x3FFF
            self.ctrl_pending = True
        else:
            self.ctrl_pending = False
            self.code |= ((value >> 4) & 0x0F) << 2
            self.address |= (value & 0x03) << 14
            if (self.code & CD_DMA) and (self.regs[1] & 0x10):
                self._trigger_dma()

    @property
    def auto_inc(self) -> int:
        return self.regs[15]

    # ---------------------------------------------------------------- data
    def _write_dest(self, value: int):
        cd = self.code & 0x07
        if cd == 0x03:              # CRAM
            self.cram[(self.address >> 1) & 0x3F] = value & 0x0FFF
        elif cd == 0x05:            # VSRAM
            self.vsram[(self.address >> 1) % 40] = value & 0x07FF
        else:                       # VRAM
            a = self.address & 0xFFFF
            self.vram[a] = (value >> 8) & 0xFF
            self.vram[(a + 1) & 0xFFFF] = value & 0xFF
        self.address = (self.address + self.auto_inc) & 0xFFFF

    def _data_write(self, value: int):
        if self.dma_fill_pending:
            self.dma_fill_pending = False
            self._dma_fill(value)
            return
        self._write_dest(value)

    def _data_read(self) -> int:
        cd = self.code & 0x0F
        addr = self.address
        if cd == 0x08:              # CRAM read
            value = self.cram[(addr >> 1) & 0x3F]
        elif cd == 0x04:            # VSRAM read
            value = self.vsram[(addr >> 1) % 40]
        else:                       # VRAM read
            a = addr & 0xFFFF
            value = (self.vram[a] << 8) | self.vram[(a + 1) & 0xFFFF]
        self.address = (self.address + self.auto_inc) & 0xFFFF
        return value

    # ----------------------------------------------------------------- DMA
    def _dma_length(self) -> int:
        length = self.regs[19] | (self.regs[20] << 8)
        return length if length else 0x10000

    def _trigger_dma(self):
        if not (self.regs[23] & 0x80):          # 68k memory -> VDP
            self._dma_transfer()
        elif not (self.regs[23] & 0x40):        # VRAM fill (waits for data)
            self.dma_fill_pending = True
        else:                                   # VRAM copy
            self._dma_copy()

    def _dma_transfer(self):
        if self.bus is None:
            return
        length = self._dma_length()
        source = ((self.regs[21] | (self.regs[22] << 8)
                   | ((self.regs[23] & 0x7F) << 16)) << 1) & 0xFFFFFF
        for _ in range(length):
            self._write_dest(self.bus.read16(source))
            source = (source + 2) & 0xFFFFFF
        self.status &= ~0x0002

    def _dma_fill(self, value: int):
        length = self._dma_length()
        a = self.address & 0xFFFF
        self.vram[a] = (value >> 8) & 0xFF
        self.vram[(a + 1) & 0xFFFF] = value & 0xFF
        fill = (value >> 8) & 0xFF
        self.address = (self.address + self.auto_inc) & 0xFFFF
        for _ in range(length):
            self.vram[self.address & 0xFFFF] = fill
            self.address = (self.address + self.auto_inc) & 0xFFFF

    def _dma_copy(self):
        length = self._dma_length()
        source = self.regs[21] | (self.regs[22] << 8)
        for _ in range(length):
            self.vram[self.address & 0xFFFF] = self.vram[source & 0xFFFF]
            source = (source + 1) & 0xFFFF
            self.address = (self.address + self.auto_inc) & 0xFFFF

    # ------------------------------------------------------------- timing
    def start_vblank(self):
        self.status |= 0x0008
        if self.regs[1] & 0x20:
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
        sizes = {0: 32, 1: 64, 3: 128}
        w = sizes.get(self.regs[16] & 0x03, 32)
        h = sizes.get((self.regs[16] >> 4) & 0x03, 32)
        return w, h

    @staticmethod
    def _color_to_rgb(c: int):
        r = _LEVELS[(c >> 1) & 0x7]
        g = _LEVELS[(c >> 5) & 0x7]
        b = _LEVELS[(c >> 9) & 0x7]
        return r, g, b

    def _draw_plane_line(self, fb, pal, y, base, scroll_x, scroll_y,
                         want_pri, width, pw, ph):
        vmask = ph * 8 - 1
        hmask = pw * 8 - 1
        py = (y + scroll_y) & vmask
        row_tile = py >> 3
        fine_y = py & 7
        vram = self.vram
        row_base = base + row_tile * pw * 2
        line_off = y * width * 3
        for x in range(width):
            px = (x - scroll_x) & hmask
            entry_addr = (row_base + (px >> 3) * 2) & 0xFFFF
            entry = (vram[entry_addr] << 8) | vram[(entry_addr + 1) & 0xFFFF]
            if ((entry >> 15) & 1) != want_pri:
                continue
            fine_x = px & 7
            tx = (7 - fine_x) if (entry & 0x0800) else fine_x
            ty = (7 - fine_y) if (entry & 0x1000) else fine_y
            addr = ((entry & 0x7FF) * 32 + ty * 4 + (tx >> 1)) & 0xFFFF
            byte = vram[addr]
            pix = (byte >> 4) if (tx & 1) == 0 else (byte & 0x0F)
            if pix == 0:
                continue
            r, g, b = pal[((entry >> 13) & 3) * 16 + pix]
            off = line_off + x * 3
            fb[off] = r
            fb[off + 1] = g
            fb[off + 2] = b

    def _draw_sprites(self, fb, pal, want_pri, width):
        table = (self.regs[5] & 0x7F) << 9
        vram = self.vram
        link = 0
        for _ in range(80):
            base = (table + link * 8) & 0xFFFF
            y = ((vram[base] << 8) | vram[base + 1]) & 0x3FF
            size = vram[base + 2]
            link_next = vram[base + 3] & 0x7F
            attr = (vram[base + 4] << 8) | vram[base + 5]
            x = ((vram[base + 6] << 8) | vram[base + 7]) & 0x1FF

            if ((attr >> 15) & 1) == want_pri:
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
                        col = (w_tiles - 1 - cx) if hflip else cx
                        rowt = (h_tiles - 1 - cy) if vflip else cy
                        self._blit_tile(fb, pal, sx0 + col * 8, sy0 + rowt * 8,
                                        t, palette, hflip, vflip, width)
            if link_next == 0:
                break
            link = link_next

    def _blit_tile(self, fb, pal, sx, sy, tile, palette, hflip, vflip, width):
        vram = self.vram
        base = (tile & 0x7FF) * 32
        for ty in range(8):
            py = sy + (7 - ty if vflip else ty)
            if py < 0 or py >= SCREEN_H:
                continue
            row = base + ty * 4
            line_off = py * width * 3
            for tx in range(8):
                px = sx + (7 - tx if hflip else tx)
                if px < 0 or px >= width:
                    continue
                byte = vram[(row + (tx >> 1)) & 0xFFFF]
                pix = (byte >> 4) if (tx & 1) == 0 else (byte & 0x0F)
                if pix == 0:
                    continue
                r, g, b = pal[(palette * 16 + pix) & 0x3F]
                off = line_off + px * 3
                fb[off] = r
                fb[off + 1] = g
                fb[off + 2] = b

    def render(self) -> tuple:
        """Render one frame; returns (width, height, RGB bytearray)."""
        width = self.screen_width
        fb = bytearray(width * SCREEN_H * 3)
        pal = [self._color_to_rgb(c) for c in self.cram]

        r, g, b = pal[self.regs[7] & 0x3F]
        for i in range(0, len(fb), 3):
            fb[i] = r
            fb[i + 1] = g
            fb[i + 2] = b

        if not self.display_enabled:
            return width, SCREEN_H, fb

        pw, ph = self._plane_dims()
        plane_b = (self.regs[4] & 0x07) << 13
        plane_a = (self.regs[2] & 0x38) << 10
        hbase = (self.regs[13] & 0x3F) << 10
        hs_mode = self.regs[11] & 0x03
        vscroll_a = self.vsram[0] & 0x3FF
        vscroll_b = self.vsram[1] & 0x3FF
        vram = self.vram

        def hscroll(y):
            if hs_mode == 0:
                idx = 0
            elif hs_mode == 3:
                idx = y
            elif hs_mode == 2:
                idx = y & 0xFFF8
            else:
                idx = y & 7
            off = (hbase + idx * 4) & 0xFFFF
            a = (vram[off] << 8) | vram[(off + 1) & 0xFFFF]
            bsc = (vram[(off + 2) & 0xFFFF] << 8) | vram[(off + 3) & 0xFFFF]
            return a, bsc

        # Painter's algorithm, back to front.
        for want_pri in (0, 1):
            for y in range(SCREEN_H):
                _, hb = hscroll(y)
                self._draw_plane_line(fb, pal, y, plane_b, hb,
                                      vscroll_b, want_pri, width, pw, ph)
            for y in range(SCREEN_H):
                ha, _ = hscroll(y)
                self._draw_plane_line(fb, pal, y, plane_a, ha,
                                      vscroll_a, want_pri, width, pw, ph)
            self._draw_sprites(fb, pal, want_pri, width)
        return width, SCREEN_H, fb
