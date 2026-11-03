#ifndef ETHOS_CONSOLE_VGA_H
#define ETHOS_CONSOLE_VGA_H

#include <stdbool.h>
#include <stdint.h>

#define VGA_TEXT_COLUMNS 80U
#define VGA_TEXT_ROWS 25U

bool vga_init(void);
bool vga_is_initialized(void);
void vga_clear(void);
void vga_set_color(uint8_t foreground, uint8_t background);
void vga_set_cursor_visible(bool visible);
void vga_write_char(char character);
void vga_write(const char *text);

#endif