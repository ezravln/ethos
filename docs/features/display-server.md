# Display Server

Ethos now has a kernel-side display server for text-cell windows. It composites up to four windows over the Limine framebuffer, draws their borders and titles, keeps a z-order, and routes keyboard events to the focused window. The implementation is in `display/server.c`; `console/vga.c` provides the cell renderer.

Try the demo from the shell:

```text
display
```

The window accepts keyboard input. Press `q` to close it and return to the shell.

## Kernel API

Include `display/server.h` from kernel code. The service must start after VGA initialization:

```c
if (vga_init())
{
  display_server_init();
}
```

A client creates a window using text-cell coordinates, draws into its local content area, then polls events while it owns focus:

```c
uint32_t window_id;
struct display_rect bounds = { .x = 10, .y = 4, .width = 50, .height = 16 };
if (display_window_create("Console", bounds, &window_id))
{
  display_window_draw_text(window_id, 1, 1, "Hello", 15, 0);
}
```

The 80 by 25 grid matches the present VGA renderer. Each window uses the requested outer bounds; its border and title take one row and column, leaving the inside for content. Colors use the 16-entry VGA palette. `display_window_focus()` raises a window, `display_window_fill()` paints its content cells, and `display_window_destroy()` removes it.

`display_server_poll_event()` reads a keyboard event only for the focused window. The service doesn't queue events for background clients, and it has no mouse input.

## What this doesn't do yet

This milestone keeps each client inside kernel space. User APX programs can't create windows or call display APIs through syscalls. The shell and Typer still write to the legacy VGA console directly, so they don't appear as managed windows. There is no pixel surface, mouse pointer, resize, clipping between arbitrary pixel buffers, or independent GUI session; the `display` command exists to exercise the compositor and focus path.
