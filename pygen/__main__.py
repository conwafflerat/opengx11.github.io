"""Command-line front-end for the pygen Genesis emulator.

Usage:
    python -m pygen info  <rom>                 Show cartridge header info
    python -m pygen run   <rom> [--scale N]     Run a ROM (needs pygame)
    python -m pygen frames <rom> [--frames N --out DIR]
                                                Run headless, dump PPM frames
    python -m pygen selftest                    Run the built-in 68000 test
"""

from __future__ import annotations

import argparse
import os
import sys


def cmd_info(args):
    from .rom import Rom
    rom = Rom.load(args.rom)
    print(f"Loaded {args.rom} ({len(rom.data)} bytes)")
    print(rom.header.describe())
    print(f"  Reset SP       : 0x{rom.reset_sp:08X}")
    print(f"  Reset PC       : 0x{rom.reset_pc:08X}")
    ok = rom.checksum_ok()
    print(f"  Checksum       : {'OK' if ok else 'MISMATCH'} "
          f"(computed 0x{rom.computed_checksum():04X})")
    return 0


def cmd_run(args):
    from .display import PygameDisplay, HAVE_PYGAME
    from .emulator import Genesis
    from .rom import Rom
    if not HAVE_PYGAME:
        print("pygame is not installed. Try: pip install pygame")
        print("Or use 'frames' to render headless PPM images instead.")
        return 1
    rom = Rom.load(args.rom)
    genesis = Genesis(rom)
    title = rom.header.overseas_title or rom.header.domestic_title or "pygen"
    display = PygameDisplay(scale=args.scale, title=title)
    try:
        running = True
        while running:
            running = display.pump(genesis.io)
            genesis.run_frame()
            display.show(*genesis.render())
    finally:
        display.quit()
    return 0


def cmd_frames(args):
    from .display import save_ppm
    from .emulator import Genesis
    from .rom import Rom
    rom = Rom.load(args.rom)
    genesis = Genesis(rom)
    os.makedirs(args.out, exist_ok=True)
    for i in range(args.frames):
        genesis.run_frame()
    width, height, rgb = genesis.render()
    path = os.path.join(args.out, f"frame_{args.frames:04d}.ppm")
    save_ppm(path, width, height, rgb)
    print(f"Ran {args.frames} frames; wrote {path} ({width}x{height})")
    return 0


def cmd_selftest(args):
    from .selftest import run_selftest
    return run_selftest()


def cmd_demo(args):
    from .demo import run_demo
    return run_demo(args.out)


def main(argv=None):
    parser = argparse.ArgumentParser(prog="pygen")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("info", help="show cartridge header info")
    p.add_argument("rom")
    p.set_defaults(func=cmd_info)

    p = sub.add_parser("run", help="run a ROM with a pygame window")
    p.add_argument("rom")
    p.add_argument("--scale", type=int, default=2)
    p.set_defaults(func=cmd_run)

    p = sub.add_parser("frames", help="run headless and dump a PPM frame")
    p.add_argument("rom")
    p.add_argument("--frames", type=int, default=60)
    p.add_argument("--out", default="frames")
    p.set_defaults(func=cmd_frames)

    p = sub.add_parser("selftest", help="run the built-in 68000 CPU test")
    p.set_defaults(func=cmd_selftest)

    p = sub.add_parser("demo", help="render the built-in VDP demo to a PPM")
    p.add_argument("--out", default="demo.ppm")
    p.set_defaults(func=cmd_demo)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
