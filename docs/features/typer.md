# Typer

Typer edits files in RAMFS and runs as a shell built-in in kernel context. Create a file first, then open it:

```text
touch /home/ethos/Documents/notes.c
typer /home/ethos/Documents/notes.c
```

You can type a relative name too. Since the shell always resolves relative paths from `/`, `typer hello.txt` opens `/hello.txt`.

## Editing controls

| Shortcut | Result |
| --- | --- |
| Arrow keys | Move the cursor |
| `Shift` + arrows | Extend the selection |
| `Home` / `End` | Jump to the line boundary |
| `Ctrl` + `Home` / `End` | Jump to the document boundary |
| `Ctrl+A` | Select the whole file |
| `Ctrl+C`, `Ctrl+X`, `Ctrl+V` | Copy, cut, or paste |
| `Ctrl+S` | Save to RAMFS |
| `Ctrl+Q` | Exit when no edits remain unsaved |
| `Backspace`, `Delete` | Remove text before or at the cursor |

Tab inserts four spaces. If you have unsaved edits, `Ctrl+Q` leaves the editor open and asks you to save. The clipboard lives only in the editor's memory.

## Storage and display

Typer accepts files up to 4096 bytes, the RAMFS file limit. It can't add syntax colors, undo edits, or save to disk; RAMFS clears when the machine reboots. See [RAMFS](ramfs.md) for writable paths.

The renderer compares screen cells and paints only those that changed after a keystroke. That avoids clearing the full framebuffer while you type. The editor still runs in the kernel because user programs don't have a keyboard input syscall.