# GScript Guide

GScript (gs) is a tiny scripting language that compiles to `.gxe` bytecode and runs via the `gxe` command in gsh. It lets you build simple apps without rebuilding the kernel.

## Features (current VM)
- Text output: `print`, `println`
- UI helpers: `clear()`, `title("...")`
- Input: `input("prompt")` stores user input in `last_input`
- Slots: `store(N)`, `load_print(N)`, `load_println(N)`, `slot_set(N, "...")` for N=0..15
- File load/save: `load_file("path")` -> fills slots with lines; `save_file("path", N)` -> writes first N slots as lines

## Writing GScript
- Source files end with `.gs`. Start with directives:
  ```
  #GUYOS_cmd;
  #start main;
  #include "io.gh";
  ```
- Statements end with `;` (except bare `exit`). Comments start with `#` or `//`.
- Strings support escapes: `\n`, `\"`, `\\`.
- Examples:
  ```
  title("Hello");
  clear();
  println("Enter your name:");
  input("> ");
  println("Hi:");
  println_last();
  save_file("note.txt", 1);
  ```

## Building .gxe
- Use the compiler: `python3 gs/compiler.py <input.gs> <output.gxe>`
- Place built `.gxe` files in `gxe/`. `buildfs.py` copies `gxe/*.gxe` into `/shell/gxe` on the FAT image during `make`.

## Running
- In gsh: `gxe /shell/gxe/your_app.gxe`
- Paths inside `load_file`/`save_file` are relative to the current working directory in gsh.

## Opcodes (for reference)
- PRINT, PRINTLN, CLEAR, TITLE, INPUT, PRINT_LAST, PRINTLN_LAST
- STORE, LOAD_PRINT, LOAD_PRINTLN, SLOT_SET
- LOAD_FILE (newline split), SAVE_FILE (newline join, count slots)
- EXIT

## Examples
- `gs/examples/gsedit_demo.gs`: line-based note editor (load/save `notes.txt`, up to 5 lines)
- `gs/examples/input_echo.gs`: prompt and echo
- `gs/examples/banner.gs`, `menu2.gs`, `versions.gs`: UI demos

## Limitations / Roadmap
- No loops/branches or cursor/key events yet; editing is line-oriented.
- File IO is plain text, newline-delimited; buffers are small (~4KB per load/save).
- Future work: control flow, key events, cursor positioning, richer file IO for a full GS-based tedit.
