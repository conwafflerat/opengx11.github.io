"""Sega Genesis / Mega Drive cartridge loading and header parsing.

The cartridge header lives at offset 0x100 in the ROM image. The first two
32-bit vectors at offset 0 are the initial supervisor stack pointer (0x00)
and the initial program counter / reset vector (0x04).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass


def _ascii(data: bytes) -> str:
    return data.decode("ascii", errors="replace").strip()


@dataclass
class RomHeader:
    console: str
    copyright: str
    domestic_title: str
    overseas_title: str
    serial: str
    checksum: int
    io_support: str
    rom_start: int
    rom_end: int
    ram_start: int
    ram_end: int
    region: str

    def describe(self) -> str:
        lines = [
            f"  Console        : {self.console}",
            f"  Copyright      : {self.copyright}",
            f"  Domestic title : {self.domestic_title}",
            f"  Overseas title : {self.overseas_title}",
            f"  Serial         : {self.serial}",
            f"  Checksum       : 0x{self.checksum:04X}",
            f"  I/O support    : {self.io_support}",
            f"  ROM range      : 0x{self.rom_start:06X} - 0x{self.rom_end:06X}",
            f"  RAM range      : 0x{self.ram_start:06X} - 0x{self.ram_end:06X}",
            f"  Region         : {self.region}",
        ]
        return "\n".join(lines)


class Rom:
    """A loaded cartridge image."""

    def __init__(self, data: bytes):
        # Some dumps use the interleaved ".smd" format; we only support plain
        # binary (.bin/.md/.gen). Detect the obvious SMD case and warn.
        if len(data) % 16384 == 512:
            raise ValueError(
                "This looks like an interleaved .smd dump (512-byte header). "
                "Convert it to a plain binary ROM first."
            )
        self.data = data
        self.header = self._parse_header(data)

    @classmethod
    def load(cls, path: str) -> "Rom":
        with open(path, "rb") as fh:
            return cls(fh.read())

    @staticmethod
    def _parse_header(data: bytes) -> RomHeader:
        if len(data) < 0x200:
            raise ValueError("ROM too small to contain a Genesis header")

        def be32(off: int) -> int:
            return struct.unpack_from(">I", data, off)[0]

        def be16(off: int) -> int:
            return struct.unpack_from(">H", data, off)[0]

        return RomHeader(
            console=_ascii(data[0x100:0x110]),
            copyright=_ascii(data[0x110:0x120]),
            domestic_title=_ascii(data[0x120:0x150]),
            overseas_title=_ascii(data[0x150:0x180]),
            serial=_ascii(data[0x180:0x18E]),
            checksum=be16(0x18E),
            io_support=_ascii(data[0x190:0x1A0]),
            rom_start=be32(0x1A0),
            rom_end=be32(0x1A4),
            ram_start=be32(0x1A8),
            ram_end=be32(0x1AC),
            region=_ascii(data[0x1F0:0x1F3]),
        )

    @property
    def reset_sp(self) -> int:
        return struct.unpack_from(">I", self.data, 0x00)[0]

    @property
    def reset_pc(self) -> int:
        return struct.unpack_from(">I", self.data, 0x04)[0]

    def computed_checksum(self) -> int:
        """Genesis checksum: sum of every 16-bit word from 0x200 to end."""
        total = 0
        for off in range(0x200, len(self.data) - 1, 2):
            total = (total + struct.unpack_from(">H", self.data, off)[0]) & 0xFFFF
        return total

    def checksum_ok(self) -> bool:
        return self.computed_checksum() == self.header.checksum
