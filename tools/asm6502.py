#!/usr/bin/env python3
import ast
import re
import sys

OPS = {
    "ADC": {"imm": 0x69, "zp": 0x65, "zpx": 0x75, "abs": 0x6D, "absx": 0x7D, "absy": 0x79, "izx": 0x61, "izy": 0x71},
    "AND": {"imm": 0x29, "zp": 0x25, "zpx": 0x35, "abs": 0x2D, "absx": 0x3D, "absy": 0x39, "izx": 0x21, "izy": 0x31},
    "ASL": {"acc": 0x0A, "zp": 0x06, "zpx": 0x16, "abs": 0x0E, "absx": 0x1E},
    "BCC": {"rel": 0x90}, "BCS": {"rel": 0xB0}, "BEQ": {"rel": 0xF0}, "BMI": {"rel": 0x30},
    "BNE": {"rel": 0xD0}, "BPL": {"rel": 0x10}, "BVC": {"rel": 0x50}, "BVS": {"rel": 0x70},
    "BIT": {"zp": 0x24, "abs": 0x2C},
    "CLC": {"imp": 0x18}, "CLD": {"imp": 0xD8}, "CLI": {"imp": 0x58}, "CLV": {"imp": 0xB8},
    "CMP": {"imm": 0xC9, "zp": 0xC5, "zpx": 0xD5, "abs": 0xCD, "absx": 0xDD, "absy": 0xD9, "izx": 0xC1, "izy": 0xD1},
    "CPX": {"imm": 0xE0, "zp": 0xE4, "abs": 0xEC}, "CPY": {"imm": 0xC0, "zp": 0xC4, "abs": 0xCC},
    "DEC": {"zp": 0xC6, "zpx": 0xD6, "abs": 0xCE, "absx": 0xDE},
    "DEX": {"imp": 0xCA}, "DEY": {"imp": 0x88},
    "EOR": {"imm": 0x49, "zp": 0x45, "zpx": 0x55, "abs": 0x4D, "absx": 0x5D, "absy": 0x59, "izx": 0x41, "izy": 0x51},
    "INC": {"zp": 0xE6, "zpx": 0xF6, "abs": 0xEE, "absx": 0xFE},
    "INX": {"imp": 0xE8}, "INY": {"imp": 0xC8},
    "JMP": {"abs": 0x4C, "ind": 0x6C}, "JSR": {"abs": 0x20},
    "LDA": {"imm": 0xA9, "zp": 0xA5, "zpx": 0xB5, "abs": 0xAD, "absx": 0xBD, "absy": 0xB9, "izx": 0xA1, "izy": 0xB1},
    "LDX": {"imm": 0xA2, "zp": 0xA6, "zpy": 0xB6, "abs": 0xAE, "absy": 0xBE},
    "LDY": {"imm": 0xA0, "zp": 0xA4, "zpx": 0xB4, "abs": 0xAC, "absx": 0xBC},
    "LSR": {"acc": 0x4A, "zp": 0x46, "zpx": 0x56, "abs": 0x4E, "absx": 0x5E},
    "NOP": {"imp": 0xEA}, "ORA": {"imm": 0x09, "zp": 0x05, "zpx": 0x15, "abs": 0x0D, "absx": 0x1D, "absy": 0x19, "izx": 0x01, "izy": 0x11},
    "PHA": {"imp": 0x48}, "PHP": {"imp": 0x08}, "PLA": {"imp": 0x68}, "PLP": {"imp": 0x28},
    "ROL": {"acc": 0x2A, "zp": 0x26, "zpx": 0x36, "abs": 0x2E, "absx": 0x3E},
    "ROR": {"acc": 0x6A, "zp": 0x66, "zpx": 0x76, "abs": 0x6E, "absx": 0x7E},
    "RTI": {"imp": 0x40}, "RTS": {"imp": 0x60},
    "SBC": {"imm": 0xE9, "zp": 0xE5, "zpx": 0xF5, "abs": 0xED, "absx": 0xFD, "absy": 0xF9, "izx": 0xE1, "izy": 0xF1},
    "SEC": {"imp": 0x38}, "SED": {"imp": 0xF8}, "SEI": {"imp": 0x78},
    "STA": {"zp": 0x85, "zpx": 0x95, "abs": 0x8D, "absx": 0x9D, "absy": 0x99, "izx": 0x81, "izy": 0x91},
    "STX": {"zp": 0x86, "zpy": 0x96, "abs": 0x8E}, "STY": {"zp": 0x84, "zpx": 0x94, "abs": 0x8C},
    "TAX": {"imp": 0xAA}, "TAY": {"imp": 0xA8}, "TSX": {"imp": 0xBA}, "TXA": {"imp": 0x8A}, "TXS": {"imp": 0x9A}, "TYA": {"imp": 0x98},
}

BRANCHES = {"BCC", "BCS", "BEQ", "BMI", "BNE", "BPL", "BVC", "BVS"}

def strip_comment(line):
    return line.split(";", 1)[0].strip()

def translate_expr(expr):
    expr = expr.replace("$", "0x")
    expr = re.sub(r"<\s*([A-Za-z_][A-Za-z0-9_]*|0x[0-9A-Fa-f]+|\d+)", r"lo(\1)", expr)
    expr = re.sub(r">\s*([A-Za-z_][A-Za-z0-9_]*|0x[0-9A-Fa-f]+|\d+)", r"hi(\1)", expr)
    return expr

def eval_expr(expr, symbols):
    def lo(x): return x & 0xFF
    def hi(x): return (x >> 8) & 0xFF
    env = {"lo": lo, "hi": hi, **symbols}
    tree = ast.parse(translate_expr(expr), mode="eval")
    for node in ast.walk(tree):
        if not isinstance(node, (ast.Expression, ast.BinOp, ast.UnaryOp, ast.Add, ast.Sub, ast.Mult, ast.Div,
                                ast.Mod, ast.USub, ast.UAdd, ast.Constant, ast.Name, ast.Load, ast.Call)):
            raise ValueError(f"Unsupported expression: {expr}")
    return int(eval(compile(tree, "<expr>", "eval"), {"__builtins__": {}}, env)) & 0xFFFF

def parse_source(path):
    lines = []
    constants = {}
    for raw in open(path, encoding="utf-8"):
        line = strip_comment(raw)
        if not line:
            continue
        if "=" in line and not line.startswith("."):
            name, expr = [part.strip() for part in line.split("=", 1)]
            constants[name] = eval_expr(expr, constants)
            continue
        lines.append(line)
    return constants, lines

def split_label(line):
    if ":" in line:
        label, rest = line.split(":", 1)
        return label.strip(), rest.strip()
    parts = line.split(None, 1)
    if len(parts) == 1 and not parts[0].startswith(".") and parts[0].upper() not in OPS:
        return parts[0], ""
    return None, line

def mode_and_expr(op, operand, symbols):
    operand = operand.strip()
    if op in BRANCHES:
        return "rel", operand
    if operand == "":
        return "imp", None
    if operand.upper() == "A":
        return "acc", None
    if operand.startswith("#"):
        return "imm", operand[1:]
    compact = re.sub(r"\s+", "", operand)
    if compact.startswith("(") and compact.endswith(",X)"):
        return "izx", compact[1:-3]
    if compact.startswith("(") and compact.endswith("),Y"):
        return "izy", compact[1:-3]
    if compact.startswith("(") and compact.endswith(")"):
        return "ind", compact[1:-1]
    if op in ("JMP", "JSR"):
        return "abs", compact
    if compact.endswith(",X"):
        expr = compact[:-2]
        try:
            value = eval_expr(expr, symbols)
        except NameError:
            value = 0x100
        return ("zpx" if value <= 0xFF else "absx"), expr
    if compact.endswith(",Y"):
        expr = compact[:-2]
        try:
            value = eval_expr(expr, symbols)
        except NameError:
            value = 0x100
        return ("zpy" if value <= 0xFF else "absy"), expr
    try:
        value = eval_expr(compact, symbols)
    except NameError:
        value = 0x100
    return ("zp" if value <= 0xFF else "abs"), compact

def size_for(line, symbols):
    if line.startswith(".org"):
        return 0
    if line.startswith(".word"):
        return 2
    op, operand = (line.split(None, 1) + [""])[:2]
    mode, _ = mode_and_expr(op.upper(), operand, symbols)
    return {"imp": 1, "acc": 1, "imm": 2, "rel": 2, "zp": 2, "zpx": 2, "zpy": 2,
            "izx": 2, "izy": 2, "abs": 3, "absx": 3, "absy": 3, "ind": 3}[mode]

def assemble(path):
    symbols, lines = parse_source(path)
    pc = 0
    origin = None
    parsed = []
    for line in lines:
        label, rest = split_label(line)
        if label:
            symbols[label] = pc
        if not rest:
            continue
        if rest.startswith(".org"):
            pc = eval_expr(rest.split(None, 1)[1], symbols)
            if origin is None:
                origin = pc
            parsed.append(rest)
            continue
        parsed.append(rest)
        pc += size_for(rest, symbols)

    if origin is None:
        origin = 0
    out = bytearray()
    pc = origin
    for line in parsed:
        if line.startswith(".org"):
            new_pc = eval_expr(line.split(None, 1)[1], symbols)
            if new_pc < pc:
                raise ValueError(f".org moved backwards to ${new_pc:04X}")
            out.extend([0] * (new_pc - pc))
            pc = new_pc
            continue
        if line.startswith(".word"):
            value = eval_expr(line.split(None, 1)[1], symbols)
            out.extend([value & 0xFF, value >> 8])
            pc += 2
            continue
        op, operand = (line.split(None, 1) + [""])[:2]
        op = op.upper()
        mode, expr = mode_and_expr(op, operand, symbols)
        opcode = OPS[op][mode]
        out.append(opcode)
        if mode in ("imm", "zp", "zpx", "zpy", "izx", "izy"):
            out.append(eval_expr(expr, symbols) & 0xFF)
        elif mode in ("abs", "absx", "absy", "ind"):
            value = eval_expr(expr, symbols)
            out.extend([value & 0xFF, value >> 8])
        elif mode == "rel":
            target = eval_expr(expr, symbols)
            offset = target - (pc + 2)
            if offset < -128 or offset > 127:
                raise ValueError(f"Branch out of range at ${pc:04X}: {line}")
            out.append(offset & 0xFF)
        pc += size_for(line, symbols)
    return origin, out

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input.asm output.bin", file=sys.stderr)
        return 2
    origin, data = assemble(sys.argv[1])
    with open(sys.argv[2], "wb") as f:
        f.write(data)
    print(f"Assembled {sys.argv[1]} -> {sys.argv[2]} at ${origin:04X}, {len(data)} bytes")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
