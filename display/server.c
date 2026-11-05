#include <display/server.h>
#include <console/vga.h>

#define DISPLAY_CELL_COUNT (VGA_TEXT_COLUMNS * VGA_TEXT_ROWS)

struct display_cell {
  char character;
  uint8_t foreground;
  uint8_t background;
};

struct display_window {
  uint32_t id;
  struct display_rect bounds;
  char title[DISPLAY_SERVER_TITLE_MAX + 1U];
  struct display_cell cells[DISPLAY_CELL_COUNT];
  bool active;
  bool visible;
};

static struct display_window windows[DISPLAY_SERVER_MAX_WINDOWS];
static uint8_t z_order[DISPLAY_SERVER_MAX_WINDOWS];
static struct display_cell composite[DISPLAY_CELL_COUNT];
static struct display_cell presented[DISPLAY_CELL_COUNT];
static size_t window_count;
static uint32_t next_window_id = 1U;
static uint32_t focused_window;
static bool initialized;
static bool presented_valid;

static bool rect_inside(struct display_rect rect, uint16_t width,
    uint16_t height)
{
  return rect.width != 0 && rect.height != 0 && rect.x < width &&
      rect.y < height && rect.width <= width - rect.x &&
      rect.height <= height - rect.y;
}

static size_t cell_index(uint16_t x, uint16_t y)
{
  return (size_t)y * VGA_TEXT_COLUMNS + x;
}

static struct display_window *find_window(uint32_t window_id)
{
  for (size_t index = 0; index < DISPLAY_SERVER_MAX_WINDOWS; index++)
  {
    if (windows[index].active && windows[index].id == window_id)
    {
      return &windows[index];
    }
  }
  return 0;
}

static void composite_cell(uint16_t x, uint16_t y, char character,
    uint8_t foreground, uint8_t background)
{
  if (x >= VGA_TEXT_COLUMNS || y >= VGA_TEXT_ROWS)
  {
    return;
  }
  composite[cell_index(x, y)] = (struct display_cell) {
    .character = character,
    .foreground = foreground,
    .background = background
  };
}

static void draw_window_frame(struct display_window *window)
{
  uint16_t x = window->bounds.x;
  uint16_t y = window->bounds.y;
  uint16_t width = window->bounds.width;
  uint16_t height = window->bounds.height;
  uint8_t title_foreground = window->id == focused_window ? 15U : 7U;
  uint8_t title_background = window->id == focused_window ? 1U : 8U;

  for (uint16_t column = 0; column < width; column++)
  {
    composite_cell(x + column, y, '-', title_foreground, title_background);
    composite_cell(x + column, y + height - 1U, '-', 7U, 0U);
  }
  for (uint16_t row = 1; row + 1U < height; row++)
  {
    composite_cell(x, y + row, '|', 7U, 0U);
    composite_cell(x + width - 1U, y + row, '|', 7U, 0U);
  }
  composite_cell(x, y, '+', title_foreground, title_background);
  composite_cell(x + width - 1U, y, '+', title_foreground, title_background);
  composite_cell(x, y + height - 1U, '+', 7U, 0U);
  composite_cell(x + width - 1U, y + height - 1U, '+', 7U, 0U);

  uint16_t title_x = x + 2U;
  for (size_t index = 0; window->title[index] != '\0' &&
      title_x < x + width - 2U; index++, title_x++)
  {
    composite_cell(title_x, y, window->title[index], title_foreground,
        title_background);
  }

  for (uint16_t row = 0; row < height - 2U; row++)
  {
    for (uint16_t column = 0; column < width - 2U; column++)
    {
      struct display_cell cell = window->cells[
          cell_index(column, row)];
      composite_cell(x + column + 1U, y + row + 1U, cell.character,
          cell.foreground, cell.background);
    }
  }
}

void display_server_present(void)
{
  if (!initialized || !vga_is_initialized())
  {
    return;
  }

  for (size_t index = 0; index < DISPLAY_CELL_COUNT; index++)
  {
    composite[index] = (struct display_cell) { ' ', 7U, 0U };
  }
  for (size_t order = 0; order < window_count; order++)
  {
    struct display_window *window = &windows[z_order[order]];
    if (window->active && window->visible)
    {
      draw_window_frame(window);
    }
  }
  vga_set_cursor_visible(false);
  for (uint16_t row = 0; row < VGA_TEXT_ROWS; row++)
  {
    for (uint16_t column = 0; column < VGA_TEXT_COLUMNS; column++)
    {
      size_t index = cell_index(column, row);
      struct display_cell cell = composite[index];
      if (presented_valid && cell.character == presented[index].character &&
          cell.foreground == presented[index].foreground &&
          cell.background == presented[index].background)
      {
        continue;
      }
      vga_draw_cell(column, row, cell.character, cell.foreground,
          cell.background);
      presented[index] = cell;
    }
  }
  presented_valid = true;
}

bool display_server_init(void)
{
  if (initialized || !vga_is_initialized())
  {
    return false;
  }
  for (size_t index = 0; index < DISPLAY_SERVER_MAX_WINDOWS; index++)
  {
    windows[index] = (struct display_window) {0};
  }
  window_count = 0;
  focused_window = 0;
  next_window_id = 1U;
  presented_valid = false;
  initialized = true;
  return true;
}

bool display_server_is_initialized(void)
{
  return initialized;
}

bool display_window_create(const char *title, struct display_rect bounds,
    uint32_t *window_id)
{
  if (!initialized || title == 0 || window_id == 0 ||
      bounds.width < 8U || bounds.height < 4U ||
      !rect_inside(bounds, VGA_TEXT_COLUMNS, VGA_TEXT_ROWS) ||
      next_window_id == 0)
  {
    return false;
  }

  size_t slot = 0;
  while (slot < DISPLAY_SERVER_MAX_WINDOWS && windows[slot].active)
  {
    slot++;
  }
  if (slot == DISPLAY_SERVER_MAX_WINDOWS ||
      window_count == DISPLAY_SERVER_MAX_WINDOWS)
  {
    return false;
  }

  struct display_window *window = &windows[slot];
  window->id = next_window_id++;
  window->bounds = bounds;
  window->active = true;
  window->visible = true;
  size_t title_length = 0;
  while (title[title_length] != '\0' &&
      title_length < DISPLAY_SERVER_TITLE_MAX)
  {
    window->title[title_length] = title[title_length];
    title_length++;
  }
  window->title[title_length] = '\0';
  for (size_t index = 0; index < DISPLAY_CELL_COUNT; index++)
  {
    window->cells[index] = (struct display_cell) { ' ', 7U, 0U };
  }
  z_order[window_count++] = (uint8_t)slot;
  focused_window = window->id;
  *window_id = window->id;
  display_server_present();
  return true;
}

bool display_window_destroy(uint32_t window_id)
{
  if (!initialized || window_id == 0)
  {
    return false;
  }
  size_t slot = DISPLAY_SERVER_MAX_WINDOWS;
  for (size_t index = 0; index < DISPLAY_SERVER_MAX_WINDOWS; index++)
  {
    if (windows[index].active && windows[index].id == window_id)
    {
      slot = index;
      break;
    }
  }
  if (slot == DISPLAY_SERVER_MAX_WINDOWS)
  {
    return false;
  }
  windows[slot].active = false;
  size_t order = 0;
  while (order < window_count && z_order[order] != slot)
  {
    order++;
  }
  if (order < window_count)
  {
    for (size_t index = order + 1U; index < window_count; index++)
    {
      z_order[index - 1U] = z_order[index];
    }
    window_count--;
  }
  if (focused_window == window_id)
  {
    focused_window = window_count == 0 ? 0U
        : windows[z_order[window_count - 1U]].id;
  }
  display_server_present();
  return true;
}

bool display_window_focus(uint32_t window_id)
{
  struct display_window *window = find_window(window_id);
  if (window == 0 || !window->visible)
  {
    return false;
  }
  size_t slot = (size_t)(window - windows);
  size_t order = 0;
  while (order < window_count && z_order[order] != slot)
  {
    order++;
  }
  if (order == window_count)
  {
    return false;
  }
  for (size_t index = order + 1U; index < window_count; index++)
  {
    z_order[index - 1U] = z_order[index];
  }
  z_order[window_count - 1U] = (uint8_t)slot;
  focused_window = window_id;
  display_server_present();
  return true;
}

bool display_window_fill(uint32_t window_id, struct display_rect area,
    char character, uint8_t foreground, uint8_t background)
{
  struct display_window *window = find_window(window_id);
  if (window == 0 || foreground >= 16U || background >= 16U ||
      !rect_inside(area, window->bounds.width - 2U,
          window->bounds.height - 2U))
  {
    return false;
  }
  for (uint16_t row = 0; row < area.height; row++)
  {
    for (uint16_t column = 0; column < area.width; column++)
    {
      window->cells[cell_index(area.x + column, area.y + row)] =
          (struct display_cell) { character, foreground, background };
    }
  }
  display_server_present();
  return true;
}

bool display_window_draw_text(uint32_t window_id, uint16_t x, uint16_t y,
    const char *text, uint8_t foreground, uint8_t background)
{
  struct display_window *window = find_window(window_id);
  if (window == 0 || text == 0 || foreground >= 16U || background >= 16U ||
      x >= window->bounds.width - 2U || y >= window->bounds.height - 2U)
  {
    return false;
  }
  uint16_t column = x;
  uint16_t row = y;
  while (*text != '\0' && row < window->bounds.height - 2U)
  {
    if (*text == '\n')
    {
      row++;
      column = x;
      text++;
      continue;
    }
    if (column >= window->bounds.width - 2U)
    {
      break;
    }
    window->cells[cell_index(column, row)] =
        (struct display_cell) { *text++, foreground, background };
    column++;
  }
  display_server_present();
  return true;
}

bool display_server_poll_event(uint32_t window_id,
    struct keyboard_event *event)
{
  if (!initialized || event == 0 || window_id != focused_window ||
      find_window(window_id) == 0)
  {
    return false;
  }
  return keyboard_poll_event(event);
}
