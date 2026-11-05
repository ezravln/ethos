#ifndef ETHOS_DISPLAY_SERVER_H
#define ETHOS_DISPLAY_SERVER_H

#include <input/keyboard.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISPLAY_SERVER_MAX_WINDOWS 4U
#define DISPLAY_SERVER_TITLE_MAX 31U

struct display_rect {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

bool display_server_init(void);
bool display_server_is_initialized(void);
bool display_window_create(const char *title, struct display_rect bounds,
    uint32_t *window_id);
bool display_window_destroy(uint32_t window_id);
bool display_window_focus(uint32_t window_id);
bool display_window_fill(uint32_t window_id, struct display_rect area,
    char character, uint8_t foreground, uint8_t background);
bool display_window_draw_text(uint32_t window_id, uint16_t x, uint16_t y,
    const char *text, uint8_t foreground, uint8_t background);
bool display_server_poll_event(uint32_t window_id,
    struct keyboard_event *event);
void display_server_present(void);

#endif
