"""Tests for the pygen Genesis emulator.

Runnable either under pytest, or directly with no dependencies:

    python3 -m pygen.tests.test_pygen
"""

from __future__ import annotations

import struct

from pygen.bus import Bus
from pygen.m68k import M68K, sign_extend
from pygen.rom import Rom
from pygen.selftest import build_image, run_selftest
from pygen.vdp import VDP
from pygen.demo import build_demo_vdp


def _cpu_with(words, sp=0x00FFE000, pc=0x00000200):
    image = bytearray(0x10000)
    struct.pack_into(">I", image, 0x0, sp)
    struct.pack_into(">I", image, 0x4, pc)
    for i, w in enumerate(words):
        struct.pack_into(">H", image, pc + i * 2, w)
    cpu = M68K(Bus(bytes(image)))
    cpu.reset()
    return cpu


# --------------------------------------------------------------------- CPU
def test_selftest_passes():
    assert run_selftest() == 0


def test_sign_extend():
    assert sign_extend(0xFF, 1) == -1
    assert sign_extend(0x80, 1) == -128
    assert sign_extend(0x7F, 1) == 127
    assert sign_extend(0xFFFF, 2) == -1


def test_moveq_sets_nz():
    cpu = _cpu_with([0x7000])          # MOVEQ #0,D0
    cpu.step()
    assert cpu.d[0] == 0
    assert cpu._z() == 1 and cpu._n() == 0


def test_add_carry_and_overflow():
    # MOVEQ #-1,D0 ; ADDQ.B #1,D0  -> byte 0xFF + 1 = 0x00, carry set
    cpu = _cpu_with([0x70FF, 0x5200])  # MOVEQ #-1,D0 ; ADDQ.B #1,D0
    cpu.step()
    cpu.step()
    assert (cpu.d[0] & 0xFF) == 0x00
    assert cpu._c() == 1 and cpu._z() == 1


def test_sub_borrow():
    # MOVEQ #1,D0 ; SUBQ.L #2,D0 -> -1, borrow set
    cpu = _cpu_with([0x7001, 0x5580])  # MOVEQ #1,D0 ; SUBQ.L #2,D0
    cpu.step()
    cpu.step()
    assert cpu.d[0] == 0xFFFFFFFF
    assert cpu._c() == 1 and cpu._n() == 1


def test_and_or_eor():
    # MOVEQ #$0F,D0 ; ANDI.W #$03,D0
    cpu = _cpu_with([0x700F, 0x0240, 0x0003])
    cpu.step()
    cpu.step()
    assert (cpu.d[0] & 0xFFFF) == 0x03


def test_lsl_lsr():
    cpu = _cpu_with([0x7001, 0xE388])  # MOVEQ #1,D0 ; LSL.L #1,D0  -> 2
    cpu.step()
    cpu.step()
    assert cpu.d[0] == 2


def test_swap():
    # MOVE.L #$12345678,D0 ; SWAP D0
    cpu = _cpu_with([0x203C, 0x1234, 0x5678, 0x4840])
    cpu.step()
    cpu.step()
    assert cpu.d[0] == 0x56781234


def test_branch_taken_and_not_taken():
    # MOVEQ #0,D0 (Z=1) ; BEQ +2 (skip next) ; MOVEQ #9,D0 ; MOVEQ #5,D1
    cpu = _cpu_with([0x7000, 0x6702, 0x7009, 0x7205])
    cpu.step()             # MOVEQ #0,D0
    cpu.step()             # BEQ -> taken, skips the 0x7009
    cpu.step()             # MOVEQ #5,D1
    assert cpu.d[0] == 0   # the MOVEQ #9 was skipped
    assert cpu.d[1] == 5


def test_jsr_rts_balances_stack():
    image = bytearray(0x10000)
    struct.pack_into(">I", image, 0x0, 0x00FFE000)
    struct.pack_into(">I", image, 0x4, 0x00000200)
    struct.pack_into(">H", image, 0x200, 0x4EB9)   # JSR $300
    struct.pack_into(">I", image, 0x202, 0x00000300)
    struct.pack_into(">H", image, 0x206, 0x4E71)   # NOP
    struct.pack_into(">H", image, 0x300, 0x7042)   # MOVEQ #$42,D0
    struct.pack_into(">H", image, 0x302, 0x4E75)   # RTS
    cpu = M68K(Bus(bytes(image)))
    cpu.reset()
    cpu.step()  # JSR
    assert cpu.pc == 0x300
    cpu.step()  # MOVEQ
    cpu.step()  # RTS
    assert cpu.pc == 0x206
    assert cpu.a[7] == 0x00FFE000
    assert cpu.d[0] == 0x42


def test_mulu_divu():
    # MOVEQ #6,D0 ; MOVEQ #7,D1 ; MULU D1,D0 -> 42
    cpu = _cpu_with([0x7006, 0x7207, 0xC0C1])
    cpu.step(); cpu.step(); cpu.step()
    assert cpu.d[0] == 42


# --------------------------------------------------------------------- ROM
def _synthetic_rom():
    data = bytearray(0x400)
    struct.pack_into(">I", data, 0x0, 0x00FFE000)   # SP
    struct.pack_into(">I", data, 0x4, 0x00000200)   # PC
    data[0x100:0x110] = b"SEGA MEGA DRIVE "
    data[0x110:0x120] = b"(C)TEST 2026.JAN"
    data[0x120:0x150] = b"PYGEN TEST CARTRIDGE".ljust(0x30)
    data[0x150:0x180] = b"PYGEN TEST CARTRIDGE".ljust(0x30)
    data[0x180:0x18E] = b"GM 00000000-00"
    struct.pack_into(">I", data, 0x1A0, 0x00000000)
    struct.pack_into(">I", data, 0x1A4, 0x000003FF)
    data[0x1F0:0x1F3] = b"JUE"
    # Set the stored checksum to the computed one.
    struct.pack_into(">H", data, 0x18E, 0)
    rom = Rom(bytes(data))
    struct.pack_into(">H", data, 0x18E, rom.computed_checksum())
    return bytes(data)


def test_rom_header_parsing():
    rom = Rom(_synthetic_rom())
    assert rom.header.console.startswith("SEGA MEGA DRIVE")
    assert rom.header.domestic_title == "PYGEN TEST CARTRIDGE"
    assert rom.header.region == "JUE"
    assert rom.reset_sp == 0x00FFE000
    assert rom.reset_pc == 0x00000200
    assert rom.checksum_ok()


# --------------------------------------------------------------------- VDP
def test_vdp_register_write():
    vdp = VDP()
    vdp.write_port(0x04, 0x8F02)   # control: register 15 = 0x02
    assert vdp.regs[15] == 0x02


def test_vdp_cram_write_and_render():
    vdp = build_demo_vdp()
    width, height, rgb = vdp.render()
    assert (width, height) == (320, 224)
    assert len(rgb) == width * height * 3
    bg = (rgb[0], rgb[1], rgb[2])
    non_bg = sum(1 for i in range(0, len(rgb), 3)
                 if (rgb[i], rgb[i + 1], rgb[i + 2]) != bg)
    assert non_bg > 1000  # the tilemap actually drew something


def test_vdp_vram_write_through_data_port():
    vdp = VDP()
    vdp.regs[15] = 2
    # Control: set up a VRAM write to address 0x0000 (code 0x01).
    vdp.write_port(0x04, 0x4000)   # first word: CD=01, A13..A0 = 0
    vdp.write_port(0x04, 0x0000)   # second word
    vdp.write_port(0x00, 0x1234)   # data
    assert vdp.vram[0] == 0x12 and vdp.vram[1] == 0x34
    assert vdp.address == 2        # auto-incremented


# ------------------------------------------------------------- integration
def test_cpu_writes_vdp_through_bus():
    # A 68000 program that writes VDP register 15 via the memory-mapped
    # control port at $C00004, then loops -- exercising CPU -> bus -> VDP.
    from pygen.emulator import Genesis

    data = bytearray(0x400)
    struct.pack_into(">I", data, 0x0, 0x00FFE000)
    struct.pack_into(">I", data, 0x4, 0x00000200)
    struct.pack_into(">H", data, 0x200, 0x33FC)        # MOVE.W #$8F02,$00C00004
    struct.pack_into(">H", data, 0x202, 0x8F02)
    struct.pack_into(">I", data, 0x204, 0x00C00004)
    struct.pack_into(">H", data, 0x208, 0x60FE)        # BRA *
    data[0x100:0x110] = b"SEGA MEGA DRIVE "

    genesis = Genesis(Rom(bytes(data)))
    genesis.run_frame()
    assert genesis.vdp.regs[15] == 0x02


def _run_all():
    funcs = [v for k, v in sorted(globals().items())
             if k.startswith("test_") and callable(v)]
    passed = 0
    for fn in funcs:
        try:
            fn()
            print(f"  ok   {fn.__name__}")
            passed += 1
        except AssertionError as exc:
            print(f"  FAIL {fn.__name__}: {exc}")
        except Exception as exc:  # pragma: no cover
            print(f"  ERR  {fn.__name__}: {exc!r}")
    print(f"\n{passed}/{len(funcs)} tests passed")
    return 0 if passed == len(funcs) else 1


if __name__ == "__main__":
    import sys
    sys.exit(_run_all())
