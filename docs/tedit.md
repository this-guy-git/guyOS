# tedit (Tiny Editor) Usage

tedit is a minimal, nano‑style fullscreen editor built into guyOS. Use it when you need to view or edit plain text files from the shell.

## Launch

```
tedit <path/to/file>
```

- Opens an existing file or creates a new one if it does not exist.
- The title bar in the top-left switches to `tedit` while the editor is active and returns to `guyOS kernel shell` when you exit.

## Controls

- `Ctrl+S` — Save the current buffer.
- `Ctrl+Q` — Quit (warns once if there are unsaved changes).
- Arrow keys — Move the cursor.
- `Home` / `End` — Jump to start/end of the current line.
- `Backspace` / `Delete` — Remove characters.
- `Enter` — Insert a newline.

## Display

- Line numbers are shown on the left.
- The cursor is an underscore positioned inline with the text.
- The viewport shows a fixed number of rows; it scrolls as needed to keep the cursor visible.

## Notes & Limitations

- No search, replace, or multi-buffer support (yet).
- No syntax highlighting.
- Files are limited to the in-memory buffer size (currently 8 KB).
- Cursor blinking is static (always visible) since there is no timer-driven redraw.

## Tips

- **Quick save cadence**: hit `Ctrl+S` after major edits; there’s no autosave.
- **Line navigation**: use `Home`/`End` to jump within a line; `Ctrl+Q` twice exits when dirty.
- **Buffer limits**: if a file is too large, trim it on host or split into smaller parts before editing.
- **Shell prompt**: when tedit exits, the shell title returns to `guyOS kernel shell`.

## Development Notes

- The editor runs entirely in the kernel shell; no userland processes.
- Keyboard handling uses the shell’s raw keycodes; cursor is an inline underscore (non-blinking).
- Rendering is text-mode only: no syntax highlighting, no tabs-to-spaces conversion.
- Command availability is gated by `/shell/cmd/tedit` stub presence in the filesystem.

## How to Add tedit Command Files

1) Ensure `/shell/cmd` exists on the FAT partition (created at boot or via build scripts).
2) Place a stub file named `tedit` inside `/shell/cmd` (the build scripts typically create stubs).
3) If the command is missing, recreate stubs by rerunning the filesystem population step or manually creating the file with `touch /shell/cmd/tedit`.

## Related Commands

- `ls` — verify `/shell/cmd` and stubs.
- `cat` — view text files quickly.
- `touch` — create empty files before editing.
- `cd` — change directories before launching `tedit`.

## Troubleshooting

- **Title stays on `tedit` after exit**: ensure you exit with `Ctrl+Q`; the shell will restore the title when the editor returns.
- **File not saving**: check that the filesystem is writable and that `/shell/cmd/tedit` exists (command stubs are required). Use `ls /shell/cmd` to verify.
- **Missing command files**: rebuild or re-run the filesystem population step to recreate the stubs in `/shell/cmd`.
