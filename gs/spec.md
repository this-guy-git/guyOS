# GScript Specification (draft)

## Source (.gs)
- Plain text, UTF-8.
- Comments start with `#` and extend to end of line.
- Strings: double-quoted, with `\n`, `\"`, and `\\` escapes.
- Current implemented statements (v0):
  - `print "text";` / `println "text";` (or with parentheses)
  - `clear();`
  - `title("text");`
  - `input("prompt");` (stores last input)
  - `print_last();` / `println_last();`
  - `store(N);` / `load_print(N);` / `load_println(N);` for N=0..15
  - `slot_set(N, "text");` // set slot from literal
  - `load_file("path");` // load file lines into slots
  - `save_file("path", N);` // save first N slots as lines
  - `exit` / `return ...;`
- Planned syntax (see `gs/syntax.md` for examples):
  - Directives: `#GUYOS_cmd`, `#start main;`, `#include "io.gh";`
  - Functions: `<type> name(...) { ... }` with `return <value>;`
  - Types (initial set): `int`, `str`, `void`.
  - Headers (`.gh`): collections of function/variable declarations, no entry point.

## Bytecode (.gxe)
Little-endian layout:
```
struct Header {
  char magic[4];   // "GXE\0"
  uint16_t version; // 0x0001
  uint16_t reserved; // 0
  uint32_t code_size; // bytes of code that follow
  uint32_t string_size; // bytes of string table that follow
};
// Code bytes follow, then string table (concatenated, each 0-terminated)
```

### Opcodes (1 byte)
- 0x01 PRINT idx   — print string at index `idx` (uint16)
- 0x02 PRINTLN idx — print string + newline (uint16)
- 0x03 CLEAR       — clear body area and redraw chrome
- 0x04 TITLE idx   — set title bar to string at `idx` (uint16)
- 0x05 INPUT idx   — prompt with string `idx`, store into last_input buffer
- 0x06 PRINT_LAST  — print last_input (no newline)
- 0x07 PRINTLN_LAST— print last_input with newline
- 0x08 STORE slot  — copy last_input into slot (0..15)
- 0x09 LOAD_PRINT slot   — print slot (no newline)
- 0x0A LOAD_PRINTLN slot — print slot with newline
- 0x0B SLOT_SET slot, str_idx — set slot to literal string
- 0x10 LOAD_FILE path_idx — load file into slots (split by newline)
- 0x11 SAVE_FILE path_idx, count — write first count slots as lines to file
- 0xFF EXIT        — terminate

### String table
- Sequence of null-terminated strings. `idx` refers to the Nth string in this list (0-based).

## Validation rules
- Magic must be `GXE\0`.
- Version must be 1.
- code_size/string_size must fit the file.
- Opcodes must be known; indices must be in range of string count.

## Execution model (target for VM in gkern/gsh)
- Linear execution from byte 0 of code until EXIT or end.
- Host provides syscalls for printing, clear, title, simple line input (stored in last_input), 16 string slots, and basic file load/save (newline-delimited).

## Future extensions
- Variables/registers, arithmetic, comparisons, conditional jumps.
- Function calls, parameters, local stack.
- Syscalls for shell/file/time/input, colors, title, cursor positioning.
- Static data section for numbers/structs.
- Signatures/checksums for integrity.
- Parser support for directives (`#start`, `#include`), functions, headers (`.gh`), and typed locals/returns.
