#!/usr/bin/env python3
"""Minimal GScript compiler (v0) -> .gxe bytecode.

Usage:
  python3 gs/compiler.py input.gs output.gxe
"""
import sys
import struct
import os

MAGIC = b"GXE\0"
VERSION = 1

OP_PRINT   = 0x01
OP_PRINTLN = 0x02
OP_CLEAR   = 0x03
OP_TITLE   = 0x04
OP_INPUT   = 0x05
OP_PRINT_LAST = 0x06
OP_PRINTLN_LAST = 0x07
OP_STORE = 0x08      # store last_input into slot
OP_LOAD_PRINT = 0x09 # print slot (no newline)
OP_LOAD_PRINTLN = 0x0A # print slot with newline
OP_SLOT_SET = 0x0B   # set slot from literal
OP_LOAD_FILE = 0x10  # load file into slots
OP_SAVE_FILE = 0x11  # save slots to file
OP_EXIT    = 0xFF

class CompileError(Exception):
    pass

def decode_string_literal(tok: str) -> str:
    out = []
    i = 0
    while i < len(tok):
        c = tok[i]
        if c == '\\':
            i += 1
            if i >= len(tok):
                raise CompileError("incomplete escape")
            esc = tok[i]
            if esc == 'n': out.append('\n')
            elif esc == '"': out.append('"')
            elif esc == '\\': out.append('\\')
            else: raise CompileError(f"unknown escape: {esc}")
        else:
            out.append(c)
        i += 1
    return ''.join(out)

def strip_comment(line: str) -> str:
    # remove // or # comments
    for marker in ("//", "#"):
        idx = line.find(marker)
        if idx != -1:
            line = line[:idx]
    return line

def normalize_stmt(line: str) -> str:
    # remove trailing semicolon
    return line[:-1].strip() if line.endswith(";") else line

def parse_line(line: str, lineno: int):
    line = strip_comment(line).strip()
    if not line:
        return None

    # Ignore common directives / scaffolding
    if line.startswith("#GUYOS") or line.startswith("#start"):
        return None
    if line.startswith("#include"):
        return ("__include__", line)
    if line == "END":
        return None
    if line.endswith("{") or line.endswith("}"):
        return None
    # Ignore simple declarations like "int x = ..." / "str name = ..."
    if line.startswith(("int ", "str ", "void ")):
        return None

    line = normalize_stmt(line)

    # exit / return -> EXIT
    if line == "exit":
        return (OP_EXIT, None)
    if line.startswith("return"):
        return (OP_EXIT, None)

    # clear();
    if line == "clear()":
        return (OP_CLEAR, None)
    if line == "print_last()":
        return (OP_PRINT_LAST, None)
    if line == "println_last()":
        return (OP_PRINTLN_LAST, None)

    # store/load slots: store(slot); load_print(slot); load_println(slot);
    def parse_slot(prefix, kind):
        rest = line[len(prefix):].strip()
        if not rest.startswith("(") or not rest.endswith(")"):
            raise CompileError(f"line {lineno}: expected {prefix}(N)")
        inner = rest[1:-1].strip()
        if not inner.isdigit():
            raise CompileError(f"line {lineno}: expected numeric slot")
        slot = int(inner)
        if slot < 0 or slot > 15:
            raise CompileError(f"line {lineno}: slot must be 0-15")
        return (kind, slot)

    if line.startswith("store("):
        return parse_slot("store", OP_STORE)
    if line.startswith("load_println("):
        return parse_slot("load_println", OP_LOAD_PRINTLN)
    if line.startswith("load_print("):
        return parse_slot("load_print", OP_LOAD_PRINT)
    # slot_set(slot, "text")
    if line.startswith("slot_set"):
        rest = line[len("slot_set"):].strip()
        if not rest.startswith("(") or not rest.endswith(")"):
            raise CompileError(f"line {lineno}: expected slot_set(N, \"...\")")
        inner = rest[1:-1]
        parts = inner.split(",", 1)
        if len(parts) != 2:
            raise CompileError(f"line {lineno}: expected slot_set(N, \"...\")")
        slot_s = parts[0].strip()
        text_s = parts[1].strip()
        if not slot_s.isdigit():
            raise CompileError(f"line {lineno}: slot must be numeric")
        slot = int(slot_s)
        if slot < 0 or slot > 15:
            raise CompileError(f"line {lineno}: slot must be 0-15")
        if not (text_s.startswith('"') and text_s.endswith('"')):
            raise CompileError(f"line {lineno}: expected string literal")
        literal = decode_string_literal(text_s[1:-1])
        return (OP_SLOT_SET, (slot, literal))

    # load_file("path")
    if line.startswith("load_file"):
        rest = line[len("load_file"):].strip()
        if not rest.startswith("(") or not rest.endswith(")"):
            raise CompileError(f"line {lineno}: expected load_file(\"path\")")
        inner = rest[1:-1].strip()
        if not (inner.startswith('"') and inner.endswith('"')):
            raise CompileError(f"line {lineno}: expected string literal")
        literal = decode_string_literal(inner[1:-1])
        return (OP_LOAD_FILE, literal)

    # save_file("path", N)
    if line.startswith("save_file"):
        rest = line[len("save_file"):].strip()
        if not rest.startswith("(") or not rest.endswith(")"):
            raise CompileError(f"line {lineno}: expected save_file(\"path\", N)")
        inner = rest[1:-1]
        parts = inner.split(",", 1)
        if len(parts) != 2:
            raise CompileError(f"line {lineno}: expected save_file(\"path\", N)")
        path_s = parts[0].strip()
        count_s = parts[1].strip()
        if not (path_s.startswith('"') and path_s.endswith('"')):
            raise CompileError(f"line {lineno}: expected string literal path")
        if not count_s.isdigit():
            raise CompileError(f"line {lineno}: expected numeric count")
        literal = decode_string_literal(path_s[1:-1])
        return (OP_SAVE_FILE, (literal, int(count_s)))

    def parse_call(prefix: str, kind):
        rest = line[len(prefix):].strip()
        if not rest.startswith("(") or not rest.endswith(")"):
            raise CompileError(f"line {lineno}: expected {prefix}(\"...\")")
        inner = rest[1:-1].strip()
        if not (inner.startswith('"') and inner.endswith('"')):
            raise CompileError(f"line {lineno}: expected string literal")
        literal = inner[1:-1]
        return (kind, decode_string_literal(literal))

    def parse_input(prefix: str):
        rest = line[len(prefix):].strip()
        if rest == "()":
            return (OP_INPUT, "")
        if not rest.startswith("(") or not rest.endswith(")"):
            raise CompileError(f"line {lineno}: expected {prefix}(\"...\")")
        inner = rest[1:-1].strip()
        if inner == "":
            return (OP_INPUT, "")
        if not (inner.startswith('"') and inner.endswith('"')):
            raise CompileError(f"line {lineno}: expected string literal")
        literal = inner[1:-1]
        return (OP_INPUT, decode_string_literal(literal))

    if line.startswith("title"):
        return parse_call("title", OP_TITLE)
    if line.startswith("input"):
        return parse_input("input")
    if line.startswith("io.input"):
        return parse_input("io.input")
    if line.startswith("println"):
        return parse_call("println", OP_PRINTLN)
    if line.startswith("print"):
        return parse_call("print", OP_PRINT)
    if line.startswith("io.println"):
        return parse_call("io.println", OP_PRINTLN)
    if line.startswith("io.print"):
        return parse_call("io.print", OP_PRINT)

    # Unknown but harmless: ignore to allow forward-compat syntax
    return None

def compile_file(path: str, include_paths=None, seen=None):
    if include_paths is None:
        include_paths = []
    if seen is None:
        seen = set()

    code = []
    strings = []
    intern = {}

    def add_str(s: str) -> int:
        if s not in intern:
            intern[s] = len(strings)
            strings.append(s)
        return intern[s]

    def handle_file(fpath: str):
        if fpath in seen:
            return
        seen.add(fpath)
        with open(fpath, 'r', encoding='utf-8') as f:
            for lineno, raw in enumerate(f, 1):
                stmt = parse_line(raw, lineno)
                if stmt is None:
                    continue
                if stmt[0] == "__include__":
                    inc_line = stmt[1]
                    # format: #include "foo.gh"
                    start = inc_line.find('"')
                    end = inc_line.rfind('"')
                    if start == -1 or end == -1 or end <= start:
                        raise CompileError(f"{fpath}:{lineno}: malformed include")
                    inc_name = inc_line[start+1:end]
                    # search include paths and current dir
                    search = [os.path.dirname(fpath)] + include_paths
                    inc_path = None
                    for base in search:
                        cand = os.path.join(base, inc_name)
                        if os.path.isfile(cand):
                            inc_path = cand
                            break
                    if not inc_path:
                        raise CompileError(f"{fpath}:{lineno}: include not found: {inc_name}")
                    handle_file(inc_path)
                    continue
                op, payload = stmt
                if op in (OP_EXIT, OP_CLEAR, OP_PRINT_LAST, OP_PRINTLN_LAST, OP_STORE, OP_LOAD_PRINT, OP_LOAD_PRINTLN):
                    code.append((op, payload))
                elif op == OP_INPUT:
                    idx = add_str(payload if payload is not None else "")
                    code.append((op, idx))
                elif op == OP_SLOT_SET:
                    slot, lit = payload
                    idx = add_str(lit)
                    code.append((op, (slot, idx)))
                elif op == OP_LOAD_FILE:
                    idx = add_str(payload)
                    code.append((op, idx))
                elif op == OP_SAVE_FILE:
                    lit, cnt = payload
                    idx = add_str(lit)
                    code.append((op, (idx, cnt)))
                else:
                    idx = add_str(payload)
                    code.append((op, idx))

    handle_file(path)

    # implicit exit
    if not code or code[-1][0] != OP_EXIT:
        code.append((OP_EXIT, None))

    # build bytecode
    code_bytes = bytearray()
    for op, arg in code:
        code_bytes.append(op)
        if op in (OP_PRINT, OP_PRINTLN, OP_TITLE, OP_INPUT):
            code_bytes += struct.pack('<H', arg if arg is not None else 0)
        elif op in (OP_STORE, OP_LOAD_PRINT, OP_LOAD_PRINTLN):
            code_bytes.append(arg if arg is not None else 0)
        elif op == OP_SLOT_SET:
            slot, sidx = arg
            code_bytes.append(slot)
            code_bytes += struct.pack('<H', sidx)
        elif op == OP_LOAD_FILE:
            code_bytes += struct.pack('<H', arg if arg is not None else 0)
        elif op == OP_SAVE_FILE:
            sidx, cnt = arg
            code_bytes += struct.pack('<H', sidx if sidx is not None else 0)
            code_bytes.append(cnt & 0xFF)
    string_bytes = bytearray()
    for s in strings:
        string_bytes += s.encode('utf-8') + b'\0'

    header = struct.pack('<4sHHII', MAGIC, VERSION, 0, len(code_bytes), len(string_bytes))
    return header + code_bytes + string_bytes


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 1
    inp, outp = argv[1], argv[2]
    try:
        here = os.path.dirname(os.path.abspath(__file__))
        inc = [os.path.join(here, "include")]
        blob = compile_file(inp, include_paths=inc)
    except CompileError as e:
        print(f"compile error: {e}")
        return 1
    with open(outp, 'wb') as f:
        f.write(blob)
    print(f"wrote {outp} ({len(blob)} bytes)")
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))

