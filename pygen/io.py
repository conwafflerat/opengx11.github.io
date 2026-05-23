"""Controller and version-register I/O (0xA10000-0xA1001F).

Implements a single 3-button controller on port 1 using the standard TH-line
multiplexing protocol. Buttons are stored as "pressed" booleans; the byte
returned to the game is active-low (0 = pressed).
"""

from __future__ import annotations

BUTTONS = ("up", "down", "left", "right", "a", "b", "c", "start")


class IO:
    def __init__(self):
        self.buttons = {b: False for b in BUTTONS}
        self._ctrl1_data = 0x40     # TH high by default
        self.version = 0xA0         # overseas, NTSC, no expansion

    def set_button(self, name: str, pressed: bool):
        if name in self.buttons:
            self.buttons[name] = pressed

    def _pad_byte(self) -> int:
        b = self.buttons
        if self._ctrl1_data & 0x40:  # TH = 1: ?1CBRLDU
            bits = (b["c"] << 5 | b["b"] << 4 | b["right"] << 3
                    | b["left"] << 2 | b["down"] << 1 | b["up"])
            return (~bits & 0x3F) | 0x40
        else:                        # TH = 0: ?0SA00DU
            bits = (b["start"] << 5 | b["a"] << 4 | b["down"] << 1 | b["up"])
            return (~bits & 0x33)

    def read(self, addr: int) -> int:
        addr &= 0x1F
        if addr in (0x00, 0x01):     # version register
            return self.version
        if addr in (0x02, 0x03):     # data port 1
            return self._pad_byte()
        return 0xFF

    def write(self, addr: int, value: int):
        addr &= 0x1F
        if addr in (0x02, 0x03):
            self._ctrl1_data = value & 0xFF
