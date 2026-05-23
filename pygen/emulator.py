"""Top-level Genesis machine: wires CPU + bus + VDP + I/O and runs frames."""

from __future__ import annotations

from .bus import Bus
from .io import IO
from .m68k import M68K, IllegalInstruction
from .rom import Rom
from .vdp import VDP

# NTSC timing: ~7.67 MHz 68000, 262 scanlines/frame, 60 frames/sec.
LINES_PER_FRAME = 262
VISIBLE_LINES = 224
CYCLES_PER_LINE = 488


class Genesis:
    def __init__(self, rom: Rom, illegal_as_nop: bool = True):
        self.rom = rom
        self.vdp = VDP()
        self.io = IO()
        self.bus = Bus(rom.data, self.vdp, self.io)
        self.vdp.bus = self.bus
        self.cpu = M68K(self.bus)
        self.cpu.illegal_as_nop = illegal_as_nop
        self.cpu.reset()
        self.frame_count = 0

    def run_line(self, line: int):
        self.vdp.current_line = line
        target = self.cpu.cycles + CYCLES_PER_LINE
        while self.cpu.cycles < target:
            self.cpu.step()

    def run_frame(self):
        self.vdp.end_vblank()
        hint_counter = self.vdp.regs[10]
        for line in range(VISIBLE_LINES):
            self.run_line(line)
            if self.vdp.regs[0] & 0x10:        # HINT enabled (level 4)
                if hint_counter == 0:
                    hint_counter = self.vdp.regs[10]
                    self.cpu.raise_irq(4)
                else:
                    hint_counter -= 1
        # Enter vblank.
        self.vdp.start_vblank()
        if self.vdp.vint_pending:
            self.cpu.raise_irq(6)
            self.vdp.vint_pending = False
        for line in range(VISIBLE_LINES, LINES_PER_FRAME):
            self.run_line(line)
        self.frame_count += 1

    def render(self):
        return self.vdp.render()
