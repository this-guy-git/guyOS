# GScript (gs)

GScript is a tiny scripting language that compiles `.gs` source files into `.gxe` bytecode for guyOS. The VM now supports print/clear/title/input, string slots, and basic file load/save, enabling simple GS-based apps like a line-oriented editor demo.

## File layout
- `gs/spec.md` — language and bytecode specification.
- `gs/compiler.py` — compiler from `.gs` to `.gxe` (supports print/println/clear/title/input/slots/file load/save).
- `gs/include/` — headers (io, sys, str).
- `gs/examples/` — sample scripts (banner, menu, input_echo, gsedit_demo, etc.).

## Usage
```bash
python3 gs/compiler.py gs/examples/gsedit_demo.gs gxe/gsedit_demo.gxe
# put .gxe in gxe/ so buildfs copies to /shell/gxe
```

## Current language support (compiler)
- Statements: `print/println`, `clear()`, `title("...")`, `input("...")` (stores last_input), `print_last/println_last`, `store(N)/load_print(N)/load_println(N)` for N=0..15, `slot_set(N, "...")`, `load_file("path")`, `save_file("path", N)`, `exit`/`return`.
- Strings: `\n`, `\"`, `\\` escapes.
- Ignores: directives (`#GUYOS_cmd`, `#start ...`, `#include ...`), braces, simple declarations, `END`, and unknown lines (for forward compatibility).

## VM support (gxe command)
- Opcodes: PRINT, PRINTLN, CLEAR, TITLE, INPUT, PRINT_LAST, PRINTLN_LAST, STORE, LOAD_PRINT, LOAD_PRINTLN, SLOT_SET, LOAD_FILE, SAVE_FILE, EXIT.
- 16 string slots; files loaded/saved as newline-delimited text relative to current directory.

## Examples
- `gs/examples/gsedit_demo.gs` — minimal multi-line editor demo (3 lines) with load/save capability using slots.
- `gs/examples/input_echo.gs` — prompt and echo back input.
- `gs/examples/banner.gs`, `menu2.gs`, `versions.gs` — UI demos.

## Roadmap
- Add control flow (jumps/branches), cursor/key events, and richer file IO for a fuller tedit.
