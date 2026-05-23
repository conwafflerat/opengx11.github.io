"""A self-contained 68000 test program.

Hand-assembles a small routine into memory, runs it on the CPU core, and
checks the resulting register/memory state. This exercises MOVEQ, SUB, shifts,
ANDI, ADDQ, a Bcc loop, JSR/RTS, MULU, a memory store and STOP -- proving the
core executes real 68000 code without needing a ROM or pygame.
"""

from __future__ import annotations

import struct

from .bus import Bus
from .m68k import M68K

# (address, 16-bit word) program image.
PROGRAM = [
    # Reset vectors.
    (0x000, 0x00FF), (0x002, 0xE000),   # initial SP = 0x00FFE000
    (0x004, 0x0000), (0x006, 0x0200),   # initial PC = 0x00000200

    # Main routine at 0x200.
    (0x200, 0x7064),                    # MOVEQ #100,D0      D0 = 100
    (0x202, 0x721C),                    # MOVEQ #28,D1       D1 = 28
    (0x204, 0x9081),                    # SUB.L  D1,D0       D0 = 72
    (0x206, 0xE598),                    # LSL.L  #2,D0       D0 = 288
    (0x208, 0x0280), (0x20A, 0x0000), (0x20C, 0x00FF),  # ANDI.L #$FF,D0  D0 = 32
    (0x20E, 0x5080),                    # ADDQ.L #8,D0      D0 = 40
    (0x210, 0x7400),                    # MOVEQ #0,D2       D2 = 0
    (0x212, 0x7605),                    # MOVEQ #5,D3       D3 = 5
    # loop (0x214): sum 5+4+3+2+1 into D2
    (0x214, 0xD483),                    # ADD.L  D3,D2
    (0x216, 0x5383),                    # SUBQ.L #1,D3
    (0x218, 0x66FA),                    # BNE    loop        -> D2 = 15, D3 = 0
    (0x21A, 0x4EB9), (0x21C, 0x0000), (0x21E, 0x0300),  # JSR $300
    (0x220, 0x7C06),                    # MOVEQ #6,D6
    (0x222, 0x7E07),                    # MOVEQ #7,D7
    (0x224, 0xCCC7),                    # MULU   D7,D6       D6 = 42
    (0x226, 0x23C6), (0x228, 0x00FF), (0x22A, 0x0000),  # MOVE.L D6,$00FF0000
    (0x22C, 0x4E72), (0x22E, 0x2700),   # STOP   #$2700

    # Subroutine at 0x300.
    (0x300, 0x283C), (0x302, 0x0000), (0x304, 0xDEAD),  # MOVE.L #$DEAD,D4
    (0x306, 0x4E75),                    # RTS
]


def build_image(size: int = 0x1000) -> bytes:
    image = bytearray(size)
    for addr, word in PROGRAM:
        struct.pack_into(">H", image, addr, word)
    return bytes(image)


def run_selftest() -> int:
    bus = Bus(build_image())
    cpu = M68K(bus)
    cpu.reset()

    for _ in range(10000):
        cpu.step()
        if cpu.stopped:
            break

    checks = [
        ("D0", cpu.d[0], 40),
        ("D1", cpu.d[1], 28),
        ("D2", cpu.d[2], 15),
        ("D3", cpu.d[3], 0),
        ("D4", cpu.d[4], 0xDEAD),
        ("D6", cpu.d[6], 42),
        ("D7", cpu.d[7], 7),
        ("A7 (stack balanced)", cpu.a[7], 0x00FFE000),
        ("RAM[$FF0000].L", bus.read32(0xFF0000), 42),
        ("stopped", int(cpu.stopped), 1),
    ]

    ok = True
    print("68000 self-test")
    print("-" * 40)
    for name, got, want in checks:
        status = "ok " if got == want else "FAIL"
        if got != want:
            ok = False
        print(f"  [{status}] {name:<22} = {got:<10} (expected {want})")
    print("-" * 40)
    print("RESULT:", "PASS" if ok else "FAILURE")
    return 0 if ok else 1
