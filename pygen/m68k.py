"""A Motorola 68000 (68k) CPU core.

This is a *simple* interpreter, not a cycle-accurate one. It implements the
common instruction set used by the vast majority of Genesis game code:
MOVE/MOVEA/MOVEQ, the ALU ops (ADD/SUB/AND/OR/EOR/CMP and their immediate and
quick forms), bit ops, shifts/rotates, MULU/MULS/DIVU/DIVS, branches, the
jump/subroutine family, MOVEM, LINK/UNLK, EXG/SWAP/EXT and exception handling
for interrupts and TRAPs.

Cycle counts are approximate -- enough to pace the video/interrupt timing of
the main loop, but not accurate enough for timing-sensitive demos.
"""

from __future__ import annotations

MASK = {1: 0xFF, 2: 0xFFFF, 4: 0xFFFFFFFF}
MSB = {1: 0x80, 2: 0x8000, 4: 0x80000000}

# Condition code register bit positions (low byte of SR).
C_BIT = 0x01
V_BIT = 0x02
Z_BIT = 0x04
N_BIT = 0x08
X_BIT = 0x10


def sign_extend(value: int, size: int) -> int:
    bits = size * 8
    value &= (1 << bits) - 1
    if value & (1 << (bits - 1)):
        value -= 1 << bits
    return value


class IllegalInstruction(Exception):
    def __init__(self, opcode: int, pc: int):
        super().__init__(f"Illegal/unimplemented opcode 0x{opcode:04X} at 0x{pc:06X}")
        self.opcode = opcode
        self.pc = pc


class Operand:
    """A decoded effective address: data reg, addr reg, memory or immediate."""

    __slots__ = ("kind", "reg", "addr", "val", "size")

    def __init__(self):
        self.kind = "?"
        self.reg = 0
        self.addr = 0
        self.val = 0
        self.size = 2


class M68K:
    def __init__(self, bus):
        self.bus = bus
        self.d = [0] * 8          # data registers
        self.a = [0] * 8          # address registers (a[7] = active stack ptr)
        self.usp = 0              # user stack pointer (saved when supervisor)
        self.pc = 0
        self.sr = 0x2700          # supervisor, interrupts masked
        self.stopped = False
        self.pending_irq = 0      # highest pending interrupt level (0 = none)
        self.illegal_as_nop = False
        self.cycles = 0

    # ------------------------------------------------------------------ state
    def reset(self):
        self.a[7] = self.bus.read32(0x000000)
        self.pc = self.bus.read32(0x000004)
        self.sr = 0x2700
        self.stopped = False

    @property
    def supervisor(self) -> bool:
        return bool(self.sr & 0x2000)

    def _set_supervisor(self, on: bool):
        if on and not self.supervisor:
            self.usp, self.a[7] = self.a[7], self.usp
            self.sr |= 0x2000
        elif not on and self.supervisor:
            self.usp, self.a[7] = self.a[7], self.usp
            self.sr &= ~0x2000

    # ----------------------------------------------------------------- ccr
    def _c(self) -> int: return 1 if self.sr & C_BIT else 0
    def _v(self) -> int: return 1 if self.sr & V_BIT else 0
    def _z(self) -> int: return 1 if self.sr & Z_BIT else 0
    def _n(self) -> int: return 1 if self.sr & N_BIT else 0
    def _x(self) -> int: return 1 if self.sr & X_BIT else 0

    def _set_bit(self, bit: int, on: bool):
        if on:
            self.sr |= bit
        else:
            self.sr &= ~bit

    def set_nz(self, value: int, size: int):
        value &= MASK[size]
        self._set_bit(N_BIT, bool(value & MSB[size]))
        self._set_bit(Z_BIT, value == 0)

    def set_logic_flags(self, value: int, size: int):
        self.set_nz(value, size)
        self.sr &= ~(V_BIT | C_BIT)

    def set_add_flags(self, src, dst, res, size, with_x=True):
        m = MASK[size]
        sm = bool(src & MSB[size])
        dm = bool(dst & MSB[size])
        rm = bool(res & MSB[size])
        carry = (src & m) + (dst & m) > m
        overflow = (sm and dm and not rm) or (not sm and not dm and rm)
        self.set_nz(res, size)
        self._set_bit(V_BIT, overflow)
        self._set_bit(C_BIT, carry)
        if with_x:
            self._set_bit(X_BIT, carry)

    def set_sub_flags(self, src, dst, res, size, with_x=True):
        m = MASK[size]
        sm = bool(src & MSB[size])
        dm = bool(dst & MSB[size])
        rm = bool(res & MSB[size])
        borrow = (dst & m) < (src & m)
        overflow = (not sm and dm and not rm) or (sm and not dm and rm)
        self.set_nz(res, size)
        self._set_bit(V_BIT, overflow)
        self._set_bit(C_BIT, borrow)
        if with_x:
            self._set_bit(X_BIT, borrow)

    # ---------------------------------------------------------------- fetch
    def fetch16(self) -> int:
        v = self.bus.read16(self.pc)
        self.pc = (self.pc + 2) & 0xFFFFFFFF
        return v

    def fetch32(self) -> int:
        v = self.bus.read32(self.pc)
        self.pc = (self.pc + 4) & 0xFFFFFFFF
        return v

    # ------------------------------------------------------- register access
    def read_dn(self, reg: int, size: int) -> int:
        return self.d[reg] & MASK[size]

    def write_dn(self, reg: int, size: int, value: int):
        m = MASK[size]
        self.d[reg] = ((self.d[reg] & ~m) | (value & m)) & 0xFFFFFFFF

    # ----------------------------------------------------- effective address
    def _index(self, base: int) -> int:
        ext = self.fetch16()
        is_addr = (ext >> 15) & 1
        rn = (ext >> 12) & 7
        is_long = (ext >> 11) & 1
        disp = sign_extend(ext & 0xFF, 1)
        idx = self.a[rn] if is_addr else self.d[rn]
        idx = sign_extend(idx, 4) if is_long else sign_extend(idx & 0xFFFF, 2)
        return (base + disp + idx) & 0xFFFFFFFF

    def decode_ea(self, mode: int, reg: int, size: int) -> Operand:
        op = Operand()
        op.size = size
        if mode == 0:
            op.kind, op.reg = "D", reg
        elif mode == 1:
            op.kind, op.reg = "A", reg
        elif mode == 2:
            op.kind, op.addr = "M", self.a[reg]
        elif mode == 3:  # (An)+
            op.kind, op.addr = "M", self.a[reg]
            inc = 2 if (reg == 7 and size == 1) else size
            self.a[reg] = (self.a[reg] + inc) & 0xFFFFFFFF
        elif mode == 4:  # -(An)
            dec = 2 if (reg == 7 and size == 1) else size
            self.a[reg] = (self.a[reg] - dec) & 0xFFFFFFFF
            op.kind, op.addr = "M", self.a[reg]
        elif mode == 5:  # (d16, An)
            disp = sign_extend(self.fetch16(), 2)
            op.kind, op.addr = "M", (self.a[reg] + disp) & 0xFFFFFFFF
        elif mode == 6:  # (d8, An, Xn)
            op.kind, op.addr = "M", self._index(self.a[reg])
        elif mode == 7:
            if reg == 0:    # (xxx).W
                op.kind, op.addr = "M", sign_extend(self.fetch16(), 2) & 0xFFFFFFFF
            elif reg == 1:  # (xxx).L
                op.kind, op.addr = "M", self.fetch32()
            elif reg == 2:  # (d16, PC)
                base = self.pc
                op.kind, op.addr = "M", (base + sign_extend(self.fetch16(), 2)) & 0xFFFFFFFF
            elif reg == 3:  # (d8, PC, Xn)
                op.kind, op.addr = "M", self._index(self.pc)
            elif reg == 4:  # #immediate
                op.kind = "I"
                if size == 1:
                    op.val = self.fetch16() & 0xFF
                elif size == 2:
                    op.val = self.fetch16()
                else:
                    op.val = self.fetch32()
            else:
                raise IllegalInstruction(mode << 3 | reg, self.pc)
        else:
            raise IllegalInstruction(mode << 3 | reg, self.pc)
        return op

    def read_op(self, op: Operand) -> int:
        if op.kind == "D":
            return self.d[op.reg] & MASK[op.size]
        if op.kind == "A":
            return self.a[op.reg] & MASK[op.size]
        if op.kind == "I":
            return op.val & MASK[op.size]
        return self.bus.read(op.addr, op.size)

    def write_op(self, op: Operand, value: int):
        value &= MASK[op.size]
        if op.kind == "D":
            self.write_dn(op.reg, op.size, value)
        elif op.kind == "A":
            self.a[op.reg] = sign_extend(value, op.size) & 0xFFFFFFFF
        elif op.kind == "M":
            self.bus.write(op.addr, op.size, value)
        else:
            raise IllegalInstruction(0, self.pc)

    # --------------------------------------------------------- condition codes
    def test_cc(self, cc: int) -> bool:
        n, z, v, c = self._n(), self._z(), self._v(), self._c()
        if cc == 0x0: return True
        if cc == 0x1: return False
        if cc == 0x2: return not c and not z          # HI
        if cc == 0x3: return c or z                   # LS
        if cc == 0x4: return not c                     # CC/HS
        if cc == 0x5: return bool(c)                   # CS/LO
        if cc == 0x6: return not z                     # NE
        if cc == 0x7: return bool(z)                   # EQ
        if cc == 0x8: return not v                     # VC
        if cc == 0x9: return bool(v)                   # VS
        if cc == 0xA: return not n                     # PL
        if cc == 0xB: return bool(n)                   # MI
        if cc == 0xC: return n == v                    # GE
        if cc == 0xD: return n != v                    # LT
        if cc == 0xE: return (n == v) and not z        # GT
        if cc == 0xF: return z or (n != v)             # LE
        return False

    # ------------------------------------------------------------- exceptions
    def _push32(self, value: int):
        self.a[7] = (self.a[7] - 4) & 0xFFFFFFFF
        self.bus.write32(self.a[7], value)

    def _push16(self, value: int):
        self.a[7] = (self.a[7] - 2) & 0xFFFFFFFF
        self.bus.write16(self.a[7], value)

    def _pop32(self) -> int:
        v = self.bus.read32(self.a[7])
        self.a[7] = (self.a[7] + 4) & 0xFFFFFFFF
        return v

    def _pop16(self) -> int:
        v = self.bus.read16(self.a[7])
        self.a[7] = (self.a[7] + 2) & 0xFFFFFFFF
        return v

    def exception(self, vector: int):
        old_sr = self.sr
        self._set_supervisor(True)
        self.sr &= ~0x8000  # clear trace
        self._push32(self.pc)
        self._push16(old_sr)
        self.pc = self.bus.read32(vector * 4)
        self.stopped = False

    def interrupt(self, level: int):
        old_sr = self.sr
        self._set_supervisor(True)
        self.sr &= ~0x8000
        self._push32(self.pc)
        self._push16(old_sr)
        self.sr = (self.sr & ~0x0700) | (level << 8)  # set interrupt mask
        self.pc = self.bus.read32((24 + level) * 4)   # autovector
        self.stopped = False

    def raise_irq(self, level: int):
        if level > self.pending_irq:
            self.pending_irq = level

    def _service_irq(self):
        mask = (self.sr >> 8) & 7
        if self.pending_irq > mask or self.pending_irq == 7:
            level = self.pending_irq
            self.pending_irq = 0
            self.interrupt(level)

    # ----------------------------------------------------------------- step
    def step(self) -> int:
        self._service_irq()
        if self.stopped:
            self.cycles += 4
            return 4
        opcode = self.fetch16()
        try:
            cost = self.execute(opcode)
        except IllegalInstruction:
            if self.illegal_as_nop:
                cost = 4
            else:
                raise
        if cost is None:
            cost = 4
        self.cycles += cost
        return cost

    def execute(self, opcode: int) -> int:
        line = opcode >> 12
        return self._LINES[line](self, opcode)

    # =================================================================
    # Instruction line handlers (dispatched on the top nibble of opcode)
    # =================================================================
    def _line0(self, opcode):
        # Immediate ALU ops, bit operations, MOVEP.
        size_field = (opcode >> 6) & 3
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        op_type = (opcode >> 9) & 7

        if (opcode & 0x0100) or (opcode & 0x0F00) == 0x0800:
            return self._bit_op(opcode)

        size = {0: 1, 1: 2, 2: 4}.get(size_field)
        if size is None:
            raise IllegalInstruction(opcode, self.pc)
        imm = self._fetch_imm(size)
        dst = self.decode_ea(mode, reg, size)

        if op_type == 0:    # ORI
            if mode == 7 and reg == 4:
                return self._imm_to_ccr_sr(imm, size, "or")
            res = self.read_op(dst) | imm
            self.write_op(dst, res)
            self.set_logic_flags(res, size)
        elif op_type == 1:  # ANDI
            if mode == 7 and reg == 4:
                return self._imm_to_ccr_sr(imm, size, "and")
            res = self.read_op(dst) & imm
            self.write_op(dst, res)
            self.set_logic_flags(res, size)
        elif op_type == 2:  # SUBI
            d = self.read_op(dst)
            res = (d - imm) & MASK[size]
            self.set_sub_flags(imm, d, res, size)
            self.write_op(dst, res)
        elif op_type == 3:  # ADDI
            d = self.read_op(dst)
            res = (d + imm) & MASK[size]
            self.set_add_flags(imm, d, res, size)
            self.write_op(dst, res)
        elif op_type == 5:  # EORI
            if mode == 7 and reg == 4:
                return self._imm_to_ccr_sr(imm, size, "eor")
            res = self.read_op(dst) ^ imm
            self.write_op(dst, res)
            self.set_logic_flags(res, size)
        elif op_type == 6:  # CMPI
            d = self.read_op(dst)
            res = (d - imm) & MASK[size]
            self.set_sub_flags(imm, d, res, size)
        else:
            raise IllegalInstruction(opcode, self.pc)
        return 8

    def _imm_to_ccr_sr(self, imm, size, kind):
        if size == 1:  # to CCR
            cur = self.sr & 0xFF
            new = {"or": cur | imm, "and": cur & imm, "eor": cur ^ imm}[kind]
            self.sr = (self.sr & 0xFF00) | (new & 0xFF)
        else:          # to SR (privileged)
            if not self.supervisor:
                self.exception(8)  # privilege violation
                return 34
            new = {"or": self.sr | imm, "and": self.sr & imm, "eor": self.sr ^ imm}[kind]
            self._write_sr(new)
        return 16

    def _write_sr(self, value):
        was_super = self.supervisor
        self.sr = value & 0xFFFF
        if bool(self.sr & 0x2000) != was_super:
            # _set_supervisor swaps stack pointers; undo our raw bit set first.
            self.sr ^= 0x2000
            self._set_supervisor(not was_super)

    def _bit_op(self, opcode):
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        op_type = (opcode >> 6) & 3  # 0 BTST 1 BCHG 2 BCLR 3 BSET
        if opcode & 0x0100:  # bit number in Dn
            bit = self.d[(opcode >> 9) & 7]
        else:
            bit = self.fetch16() & 0xFF
        if mode == 0:  # operating on a data register -> 32 bits
            bit &= 31
            val = self.d[reg]
            self._set_bit(Z_BIT, not (val & (1 << bit)))
            if op_type == 1:
                self.d[reg] ^= (1 << bit)
            elif op_type == 2:
                self.d[reg] &= ~(1 << bit) & 0xFFFFFFFF
            elif op_type == 3:
                self.d[reg] |= (1 << bit)
        else:          # operating on memory -> 8 bits
            bit &= 7
            dst = self.decode_ea(mode, reg, 1)
            val = self.read_op(dst)
            self._set_bit(Z_BIT, not (val & (1 << bit)))
            if op_type == 1:
                self.write_op(dst, val ^ (1 << bit))
            elif op_type == 2:
                self.write_op(dst, val & ~(1 << bit))
            elif op_type == 3:
                self.write_op(dst, val | (1 << bit))
        return 8

    def _fetch_imm(self, size):
        if size == 1:
            return self.fetch16() & 0xFF
        if size == 2:
            return self.fetch16()
        return self.fetch32()

    def _move(self, opcode):
        sizef = opcode >> 12
        size = {1: 1, 3: 2, 2: 4}[sizef]
        src_mode = (opcode >> 3) & 7
        src_reg = opcode & 7
        dst_mode = (opcode >> 6) & 7
        dst_reg = (opcode >> 9) & 7
        src = self.decode_ea(src_mode, src_reg, size)
        value = self.read_op(src)
        if dst_mode == 1:  # MOVEA
            self.a[dst_reg] = sign_extend(value, size) & 0xFFFFFFFF
            return 4
        dst = self.decode_ea(dst_mode, dst_reg, size)
        self.write_op(dst, value)
        self.set_logic_flags(value, size)
        return 4

    def _line4(self, opcode):
        # Miscellaneous: CLR/NEG/NOT/TST/LEA/JMP/JSR/MOVEM/EXT/SWAP/...
        if (opcode & 0xFFC0) == 0x4EC0:  # JMP
            op = self.decode_ea((opcode >> 3) & 7, opcode & 7, 4)
            self.pc = op.addr
            return 8
        if (opcode & 0xFFC0) == 0x4E80:  # JSR
            op = self.decode_ea((opcode >> 3) & 7, opcode & 7, 4)
            self._push32(self.pc)
            self.pc = op.addr
            return 16
        if (opcode & 0xF1C0) == 0x41C0:  # LEA
            an = (opcode >> 9) & 7
            op = self.decode_ea((opcode >> 3) & 7, opcode & 7, 4)
            self.a[an] = op.addr
            return 4
        if (opcode & 0xFFF8) == 0x4840:  # SWAP
            r = opcode & 7
            v = self.d[r]
            self.d[r] = ((v >> 16) | (v << 16)) & 0xFFFFFFFF
            self.set_logic_flags(self.d[r], 4)
            return 4
        if (opcode & 0xFFB8) == 0x4880:  # EXT
            r = opcode & 7
            if opcode & 0x0040:  # ext.l
                self.d[r] = sign_extend(self.d[r] & 0xFFFF, 2) & 0xFFFFFFFF
                self.set_logic_flags(self.d[r], 4)
            else:               # ext.w
                v = sign_extend(self.d[r] & 0xFF, 1) & 0xFFFF
                self.write_dn(r, 2, v)
                self.set_logic_flags(v, 2)
            return 4
        if (opcode & 0xFFC0) == 0x4840:  # PEA
            op = self.decode_ea((opcode >> 3) & 7, opcode & 7, 4)
            self._push32(op.addr)
            return 12
        if opcode == 0x4E71:  # NOP
            return 4
        if opcode == 0x4E75:  # RTS
            self.pc = self._pop32()
            return 16
        if opcode == 0x4E77:  # RTR
            self.sr = (self.sr & 0xFF00) | (self._pop16() & 0xFF)
            self.pc = self._pop32()
            return 20
        if opcode == 0x4E73:  # RTE
            if not self.supervisor:
                self.exception(8)
                return 34
            new_sr = self._pop16()
            self.pc = self._pop32()
            self._write_sr(new_sr)
            return 20
        if opcode == 0x4E72:  # STOP #imm
            imm = self.fetch16()
            self._write_sr(imm)
            self.stopped = True
            return 4
        if (opcode & 0xFFF0) == 0x4E40:  # TRAP #vector
            return self._do_trap_exception(32 + (opcode & 0xF), 34)
        if (opcode & 0xFFF8) == 0x4E50:  # LINK
            an = opcode & 7
            disp = sign_extend(self.fetch16(), 2)
            self._push32(self.a[an])
            self.a[an] = self.a[7]
            self.a[7] = (self.a[7] + disp) & 0xFFFFFFFF
            return 16
        if (opcode & 0xFFF8) == 0x4E58:  # UNLK
            an = opcode & 7
            self.a[7] = self.a[an]
            self.a[an] = self._pop32()
            return 12
        if (opcode & 0xFB80) == 0x4880:  # MOVEM
            return self._movem(opcode)

        op6 = (opcode >> 8) & 0xF
        sizef = (opcode >> 6) & 3
        if op6 in (0x0, 0x2, 0x4, 0x6, 0xA) and sizef != 3:
            size = {0: 1, 1: 2, 2: 4}[sizef]
            mode = (opcode >> 3) & 7
            reg = opcode & 7
            if op6 == 0x2:  # CLR
                dst = self.decode_ea(mode, reg, size)
                self.write_op(dst, 0)
                self.sr &= ~(N_BIT | V_BIT | C_BIT)
                self.sr |= Z_BIT
                return 4
            if op6 == 0xA:  # TST
                src = self.decode_ea(mode, reg, size)
                self.set_logic_flags(self.read_op(src), size)
                return 4
            if op6 == 0x6:  # NOT
                dst = self.decode_ea(mode, reg, size)
                res = ~self.read_op(dst) & MASK[size]
                self.write_op(dst, res)
                self.set_logic_flags(res, size)
                return 4
            if op6 == 0x4:  # NEG
                dst = self.decode_ea(mode, reg, size)
                d = self.read_op(dst)
                res = (-d) & MASK[size]
                self.set_sub_flags(d, 0, res, size)
                self.write_op(dst, res)
                return 4
            if op6 == 0x0:  # NEGX
                dst = self.decode_ea(mode, reg, size)
                d = self.read_op(dst)
                res = (0 - d - self._x()) & MASK[size]
                self.set_sub_flags(d, 0, res, size)
                if res != 0:
                    self.sr &= ~Z_BIT
                self.write_op(dst, res)
                return 4
        raise IllegalInstruction(opcode, self.pc)

    def _do_trap_exception(self, vector, cost):
        self.exception(vector)
        return cost

    def _movem(self, opcode):
        direction = (opcode >> 10) & 1  # 0 reg->mem, 1 mem->reg
        size = 4 if (opcode & 0x0040) else 2
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        mask = self.fetch16()
        regs = self.d + self.a

        if direction == 0 and mode == 4:  # predecrement: A7..D0 order
            addr = self.a[reg]
            for i in range(16):
                if mask & (1 << i):
                    rnum = 15 - i
                    addr = (addr - size) & 0xFFFFFFFF
                    val = regs[rnum] & MASK[size]
                    self.bus.write(addr, size, val)
            self.a[reg] = addr
            return 8
        if direction == 1 and mode == 3:  # postincrement: D0..A7 order
            addr = self.a[reg]
            for i in range(16):
                if mask & (1 << i):
                    val = self.bus.read(addr, size)
                    if size == 2:
                        val = sign_extend(val, 2) & 0xFFFFFFFF
                    self._set_reg(i, val)
                    addr = (addr + size) & 0xFFFFFFFF
            self.a[reg] = addr
            return 8
        # Control addressing modes (always ascending D0..A7).
        op = self.decode_ea(mode, reg, size)
        addr = op.addr
        for i in range(16):
            if mask & (1 << i):
                if direction == 0:
                    self.bus.write(addr, size, regs[i] & MASK[size])
                else:
                    val = self.bus.read(addr, size)
                    if size == 2:
                        val = sign_extend(val, 2) & 0xFFFFFFFF
                    self._set_reg(i, val)
                addr = (addr + size) & 0xFFFFFFFF
        return 8

    def _set_reg(self, index, value):
        if index < 8:
            self.d[index] = value & 0xFFFFFFFF
        else:
            self.a[index - 8] = value & 0xFFFFFFFF

    def _line5(self, opcode):
        # ADDQ / SUBQ / Scc / DBcc
        if (opcode & 0x00C0) == 0x00C0:
            cc = (opcode >> 8) & 0xF
            mode = (opcode >> 3) & 7
            reg = opcode & 7
            if mode == 1:  # DBcc
                if not self.test_cc(cc):
                    counter = (self.d[reg] - 1) & 0xFFFF
                    self.write_dn(reg, 2, counter)
                    disp = sign_extend(self.fetch16(), 2)
                    if counter != 0xFFFF:
                        self.pc = (self.pc - 2 + disp) & 0xFFFFFFFF
                    return 10
                else:
                    self.fetch16()  # consume displacement
                    return 12
            else:          # Scc
                dst = self.decode_ea(mode, reg, 1)
                self.write_op(dst, 0xFF if self.test_cc(cc) else 0x00)
                return 8
        size = {0: 1, 1: 2, 2: 4}[(opcode >> 6) & 3]
        data = (opcode >> 9) & 7
        if data == 0:
            data = 8
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        if (opcode & 0x0100) == 0:  # ADDQ
            if mode == 1:  # to An: full 32-bit, no flags
                self.a[reg] = (self.a[reg] + data) & 0xFFFFFFFF
                return 8
            dst = self.decode_ea(mode, reg, size)
            d = self.read_op(dst)
            res = (d + data) & MASK[size]
            self.set_add_flags(data, d, res, size)
            self.write_op(dst, res)
        else:                        # SUBQ
            if mode == 1:
                self.a[reg] = (self.a[reg] - data) & 0xFFFFFFFF
                return 8
            dst = self.decode_ea(mode, reg, size)
            d = self.read_op(dst)
            res = (d - data) & MASK[size]
            self.set_sub_flags(data, d, res, size)
            self.write_op(dst, res)
        return 8

    def _line6(self, opcode):
        # BRA / BSR / Bcc
        cc = (opcode >> 8) & 0xF
        disp8 = opcode & 0xFF
        base = self.pc
        if disp8 == 0x00:
            disp = sign_extend(self.fetch16(), 2)
        elif disp8 == 0xFF:
            disp = sign_extend(self.fetch32(), 4)
        else:
            disp = sign_extend(disp8, 1)
        target = (base + disp) & 0xFFFFFFFF
        if cc == 0:        # BRA
            self.pc = target
        elif cc == 1:      # BSR
            self._push32(self.pc)
            self.pc = target
        else:              # Bcc
            if self.test_cc(cc):
                self.pc = target
        return 10

    def _line7(self, opcode):  # MOVEQ
        if opcode & 0x0100:
            raise IllegalInstruction(opcode, self.pc)
        reg = (opcode >> 9) & 7
        value = sign_extend(opcode & 0xFF, 1) & 0xFFFFFFFF
        self.d[reg] = value
        self.set_logic_flags(value, 4)
        return 4

    def _line8(self, opcode):  # OR / DIVU / DIVS
        op_mode = (opcode >> 6) & 7
        dn = (opcode >> 9) & 7
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        if op_mode == 3:   # DIVU
            return self._divu(dn, mode, reg, signed=False)
        if op_mode == 7:   # DIVS
            return self._divu(dn, mode, reg, signed=True)
        size = {0: 1, 1: 2, 2: 4}[op_mode & 3]
        ea = self.decode_ea(mode, reg, size)
        if op_mode < 3:    # <ea> | Dn -> Dn
            res = self.read_dn(dn, size) | self.read_op(ea)
            self.write_dn(dn, size, res)
        else:              # Dn | <ea> -> <ea>
            res = self.read_op(ea) | self.read_dn(dn, size)
            self.write_op(ea, res)
        self.set_logic_flags(res, size)
        return 4

    def _divu(self, dn, mode, reg, signed):
        ea = self.decode_ea(mode, reg, 2)
        divisor = self.read_op(ea) & 0xFFFF
        if divisor == 0:
            self.exception(5)  # zero divide
            return 38
        dividend = self.d[dn] & 0xFFFFFFFF
        if signed:
            dvd = sign_extend(dividend, 4)
            dvs = sign_extend(divisor, 2)
            q = int(dvd / dvs) if dvs else 0  # truncate toward zero
            r = dvd - q * dvs
            if q < -0x8000 or q > 0x7FFF:
                self.sr |= V_BIT
                return 158
            q &= 0xFFFF
            r &= 0xFFFF
        else:
            q = dividend // divisor
            r = dividend % divisor
            if q > 0xFFFF:
                self.sr |= V_BIT
                return 140
        self.d[dn] = ((r & 0xFFFF) << 16) | (q & 0xFFFF)
        self.set_nz(q, 2)
        self.sr &= ~(V_BIT | C_BIT)
        return 140

    def _line9(self, opcode):  # SUB / SUBA / SUBX
        op_mode = (opcode >> 6) & 7
        dn = (opcode >> 9) & 7
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        if op_mode in (3, 7):  # SUBA
            size = 2 if op_mode == 3 else 4
            ea = self.decode_ea(mode, reg, size)
            src = sign_extend(self.read_op(ea), size) & 0xFFFFFFFF
            self.a[dn] = (self.a[dn] - src) & 0xFFFFFFFF
            return 8
        size = {0: 1, 1: 2, 2: 4}[op_mode & 3]
        ea = self.decode_ea(mode, reg, size)
        if op_mode < 3:    # Dn - <ea> -> Dn
            d = self.read_dn(dn, size)
            s = self.read_op(ea)
            res = (d - s) & MASK[size]
            self.set_sub_flags(s, d, res, size)
            self.write_dn(dn, size, res)
        else:              # <ea> - Dn -> <ea>
            d = self.read_op(ea)
            s = self.read_dn(dn, size)
            res = (d - s) & MASK[size]
            self.set_sub_flags(s, d, res, size)
            self.write_op(ea, res)
        return 4

    def _lineB(self, opcode):  # CMP / CMPA / CMPM / EOR
        op_mode = (opcode >> 6) & 7
        dn = (opcode >> 9) & 7
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        if op_mode in (3, 7):  # CMPA
            size = 2 if op_mode == 3 else 4
            ea = self.decode_ea(mode, reg, size)
            s = sign_extend(self.read_op(ea), size) & 0xFFFFFFFF
            d = self.a[dn] & 0xFFFFFFFF
            res = (d - s) & 0xFFFFFFFF
            self.set_sub_flags(s, d, res, 4, with_x=False)
            return 6
        size = {0: 1, 1: 2, 2: 4}[op_mode & 3]
        if op_mode < 3:    # CMP
            ea = self.decode_ea(mode, reg, size)
            s = self.read_op(ea)
            d = self.read_dn(dn, size)
            res = (d - s) & MASK[size]
            self.set_sub_flags(s, d, res, size, with_x=False)
            return 4
        # op_mode 4..6 with mode 1 -> CMPM, otherwise EOR
        if mode == 1:      # CMPM (Ay)+,(Ax)+
            s_op = self.decode_ea(3, reg, size)
            d_op = self.decode_ea(3, dn, size)
            s = self.read_op(s_op)
            d = self.read_op(d_op)
            res = (d - s) & MASK[size]
            self.set_sub_flags(s, d, res, size, with_x=False)
            return 8
        ea = self.decode_ea(mode, reg, size)  # EOR
        res = self.read_op(ea) ^ self.read_dn(dn, size)
        self.write_op(ea, res)
        self.set_logic_flags(res, size)
        return 4

    def _lineC(self, opcode):  # AND / MULU / MULS / EXG / ABCD
        op_mode = (opcode >> 6) & 7
        dn = (opcode >> 9) & 7
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        if op_mode == 3:   # MULU
            ea = self.decode_ea(mode, reg, 2)
            res = (self.d[dn] & 0xFFFF) * (self.read_op(ea) & 0xFFFF)
            self.d[dn] = res & 0xFFFFFFFF
            self.set_nz(res, 4)
            self.sr &= ~(V_BIT | C_BIT)
            return 54
        if op_mode == 7:   # MULS
            ea = self.decode_ea(mode, reg, 2)
            res = sign_extend(self.d[dn] & 0xFFFF, 2) * sign_extend(self.read_op(ea) & 0xFFFF, 2)
            self.d[dn] = res & 0xFFFFFFFF
            self.set_nz(res & 0xFFFFFFFF, 4)
            self.sr &= ~(V_BIT | C_BIT)
            return 54
        if (opcode & 0x0130) == 0x0100:  # EXG
            mode_field = (opcode >> 3) & 0x1F
            rx = (opcode >> 9) & 7
            ry = opcode & 7
            if mode_field == 0x08:        # data <-> data
                self.d[rx], self.d[ry] = self.d[ry], self.d[rx]
            elif mode_field == 0x09:      # addr <-> addr
                self.a[rx], self.a[ry] = self.a[ry], self.a[rx]
            elif mode_field == 0x11:      # data <-> addr
                self.d[rx], self.a[ry] = self.a[ry], self.d[rx]
            else:
                raise IllegalInstruction(opcode, self.pc)
            return 6
        size = {0: 1, 1: 2, 2: 4}[op_mode & 3]
        ea = self.decode_ea(mode, reg, size)
        if op_mode < 3:    # <ea> & Dn -> Dn
            res = self.read_dn(dn, size) & self.read_op(ea)
            self.write_dn(dn, size, res)
        else:              # Dn & <ea> -> <ea>
            res = self.read_op(ea) & self.read_dn(dn, size)
            self.write_op(ea, res)
        self.set_logic_flags(res, size)
        return 4

    def _lineD(self, opcode):  # ADD / ADDA / ADDX
        op_mode = (opcode >> 6) & 7
        dn = (opcode >> 9) & 7
        mode = (opcode >> 3) & 7
        reg = opcode & 7
        if op_mode in (3, 7):  # ADDA
            size = 2 if op_mode == 3 else 4
            ea = self.decode_ea(mode, reg, size)
            src = sign_extend(self.read_op(ea), size) & 0xFFFFFFFF
            self.a[dn] = (self.a[dn] + src) & 0xFFFFFFFF
            return 8
        size = {0: 1, 1: 2, 2: 4}[op_mode & 3]
        ea = self.decode_ea(mode, reg, size)
        if op_mode < 3:    # <ea> + Dn -> Dn
            d = self.read_dn(dn, size)
            s = self.read_op(ea)
            res = (d + s) & MASK[size]
            self.set_add_flags(s, d, res, size)
            self.write_dn(dn, size, res)
        else:              # Dn + <ea> -> <ea>
            d = self.read_op(ea)
            s = self.read_dn(dn, size)
            res = (d + s) & MASK[size]
            self.set_add_flags(s, d, res, size)
            self.write_op(ea, res)
        return 4

    def _lineE(self, opcode):  # Shifts and rotates
        if (opcode & 0x00C0) == 0x00C0:  # memory shift by 1
            mode = (opcode >> 3) & 7
            reg = opcode & 7
            op = self.decode_ea(mode, reg, 2)
            val = self.read_op(op)
            direction = (opcode >> 8) & 1
            kind = (opcode >> 9) & 3
            res = self._do_shift(kind, direction, val, 2, 1)
            self.write_op(op, res)
            return 8
        size = {0: 1, 1: 2, 2: 4}[(opcode >> 6) & 3]
        direction = (opcode >> 8) & 1     # 0 right, 1 left
        reg = opcode & 7
        kind = (opcode >> 3) & 3
        if opcode & 0x0020:  # count in Dn
            count = self.d[(opcode >> 9) & 7] & 63
        else:                # immediate count (1..8)
            count = (opcode >> 9) & 7
            if count == 0:
                count = 8
        val = self.read_dn(reg, size)
        res = self._do_shift(kind, direction, val, size, count)
        self.write_dn(reg, size, res)
        return 6 + 2 * count

    def _do_shift(self, kind, direction, val, size, count):
        m = MASK[size]
        bits = size * 8
        val &= m
        self.sr &= ~(V_BIT)
        if count == 0:
            self.set_nz(val, size)
            self.sr &= ~C_BIT
            return val
        carry = 0
        if kind == 0:    # ASL/ASR (arithmetic)
            if direction:  # ASL
                overflow = False
                for _ in range(count):
                    carry = 1 if (val & MSB[size]) else 0
                    new = (val << 1) & m
                    if (new & MSB[size]) != (val & MSB[size]):
                        overflow = True
                    val = new
                self._set_bit(V_BIT, overflow)
            else:          # ASR
                sign = val & MSB[size]
                for _ in range(count):
                    carry = val & 1
                    val = (val >> 1) | sign
                val &= m
        elif kind == 1:  # LSL/LSR (logical)
            if direction:
                for _ in range(count):
                    carry = 1 if (val & MSB[size]) else 0
                    val = (val << 1) & m
            else:
                for _ in range(count):
                    carry = val & 1
                    val >>= 1
        elif kind == 2:  # ROXL/ROXR (rotate through X)
            x = self._x()
            if direction:
                for _ in range(count):
                    carry = 1 if (val & MSB[size]) else 0
                    val = ((val << 1) | x) & m
                    x = carry
            else:
                for _ in range(count):
                    carry = val & 1
                    val = (val >> 1) | (x << (bits - 1))
                    x = carry
            self._set_bit(X_BIT, bool(carry))
        else:            # ROL/ROR (rotate)
            cnt = count % bits
            if direction:
                val = ((val << cnt) | (val >> (bits - cnt))) & m if cnt else val
                carry = val & 1
            else:
                val = ((val >> cnt) | (val << (bits - cnt))) & m if cnt else val
                carry = 1 if (val & MSB[size]) else 0
        self._set_bit(C_BIT, bool(carry))
        if kind in (0, 1):  # ASL/ASR and LSL/LSR set X = C
            self._set_bit(X_BIT, bool(carry))
        # ROXL/ROXR already set X inside the loop; ROL/ROR leave X unchanged.
        self.set_nz(val, size)
        return val

    def _illegal(self, opcode):
        raise IllegalInstruction(opcode, self.pc)

    _LINES = {
        0x0: _line0,
        0x1: _move,
        0x2: _move,
        0x3: _move,
        0x4: _line4,
        0x5: _line5,
        0x6: _line6,
        0x7: _line7,
        0x8: _line8,
        0x9: _line9,
        0xA: _illegal,
        0xB: _lineB,
        0xC: _lineC,
        0xD: _lineD,
        0xE: _lineE,
        0xF: _illegal,
    }
