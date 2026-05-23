"""68000 memory bus and Genesis address map.

Genesis 24-bit address space (the parts a simple emulator cares about):

    0x000000 - 0x3FFFFF   Cartridge ROM
    0xA00000 - 0xA0FFFF   Z80 address space (stubbed)
    0xA10000 - 0xA1001F   I/O: version register and controller ports
    0xA11100              Z80 bus request
    0xA11200              Z80 reset
    0xC00000 - 0xC0001F   VDP data/control/HV ports
    0xFF0000 - 0xFFFFFF   68000 work RAM (64 KiB, mirrored from 0xE00000)
"""

from __future__ import annotations


class Bus:
    def __init__(self, rom: bytes, vdp=None, io=None):
        self.rom = rom
        self.rom_len = len(rom)
        self.ram = bytearray(0x10000)  # 64 KiB
        self.vdp = vdp
        self.io = io
        self.z80_ram = bytearray(0x2000)

    # -------------------------------------------------------------- helpers
    def read(self, addr: int, size: int) -> int:
        if size == 1:
            return self.read8(addr)
        if size == 2:
            return self.read16(addr)
        return self.read32(addr)

    def write(self, addr: int, size: int, value: int):
        if size == 1:
            self.write8(addr, value)
        elif size == 2:
            self.write16(addr, value)
        else:
            self.write32(addr, value)

    # ----------------------------------------------------------------- byte
    def read8(self, addr: int) -> int:
        addr &= 0xFFFFFF
        if addr < 0x400000:
            return self.rom[addr] if addr < self.rom_len else 0
        if 0xE00000 <= addr:
            return self.ram[addr & 0xFFFF]
        if 0xA00000 <= addr <= 0xA0FFFF:
            return self.z80_ram[addr & 0x1FFF]
        if 0xA10000 <= addr <= 0xA1001F:
            return self.io.read(addr) if self.io else 0xFF
        if 0xC00000 <= addr <= 0xC0001F:
            word = self.vdp.read_port(addr & 0x1E) if self.vdp else 0
            return (word >> 8) & 0xFF if (addr & 1) == 0 else word & 0xFF
        return 0xFF

    def write8(self, addr: int, value: int):
        addr &= 0xFFFFFF
        value &= 0xFF
        if 0xE00000 <= addr:
            self.ram[addr & 0xFFFF] = value
        elif 0xA00000 <= addr <= 0xA0FFFF:
            self.z80_ram[addr & 0x1FFF] = value
        elif 0xA10000 <= addr <= 0xA1001F:
            if self.io:
                self.io.write(addr, value)
        elif 0xC00000 <= addr <= 0xC0001F:
            if self.vdp:
                self.vdp.write_port(addr & 0x1E, value | (value << 8))
        # ROM and unmapped writes are ignored.

    # ----------------------------------------------------------------- word
    def read16(self, addr: int) -> int:
        addr &= 0xFFFFFF
        if addr < 0x400000:
            if addr + 1 < self.rom_len:
                return (self.rom[addr] << 8) | self.rom[addr + 1]
            return 0
        if 0xE00000 <= addr:
            o = addr & 0xFFFF
            return (self.ram[o] << 8) | self.ram[(o + 1) & 0xFFFF]
        if 0xC00000 <= addr <= 0xC0001F:
            return self.vdp.read_port(addr & 0x1E) if self.vdp else 0
        if 0xA10000 <= addr <= 0xA1001F:
            return self.io.read(addr) if self.io else 0xFFFF
        return (self.read8(addr) << 8) | self.read8(addr + 1)

    def write16(self, addr: int, value: int):
        addr &= 0xFFFFFF
        value &= 0xFFFF
        if 0xE00000 <= addr:
            o = addr & 0xFFFF
            self.ram[o] = value >> 8
            self.ram[(o + 1) & 0xFFFF] = value & 0xFF
        elif 0xC00000 <= addr <= 0xC0001F:
            if self.vdp:
                self.vdp.write_port(addr & 0x1E, value)
        elif 0xA10000 <= addr <= 0xA1001F:
            if self.io:
                self.io.write(addr, value & 0xFF)
        else:
            self.write8(addr, value >> 8)
            self.write8(addr + 1, value & 0xFF)

    # ----------------------------------------------------------------- long
    def read32(self, addr: int) -> int:
        return (self.read16(addr) << 16) | self.read16(addr + 2)

    def write32(self, addr: int, value: int):
        self.write16(addr, (value >> 16) & 0xFFFF)
        self.write16(addr + 2, value & 0xFFFF)
