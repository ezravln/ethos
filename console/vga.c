#include <console/vga.h>
#include <boot/boot.h>
#include <stddef.h>

#define FONT_WIDTH 5U
#define FONT_HEIGHT 7U
#define FONT_SCALE 2U
#define CELL_WIDTH 12U
#define CELL_HEIGHT 18U

static const uint8_t font[36][FONT_HEIGHT] = {
  {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
  {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
  {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F},
  {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
  {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
  {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
  {0x0F, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0F},
  {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
  {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
  {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
  {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
  {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
  {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
  {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
  {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
  {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
  {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
  {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
  {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
  {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
  {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
  {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
  {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
  {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
  {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
  {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
  {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
  {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
  {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
  {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
  {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
  {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
  {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}
};

static const uint32_t palette[16] = {
  0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
  0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
  0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
  0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static struct limine_framebuffer *framebuffer;
static uint32_t foreground;
static uint32_t background;
static uint64_t cursor_x;
static uint64_t cursor_y;
static bool console_initialized;

static uint32_t color_component(uint8_t value, uint8_t size, uint8_t shift)
{
  if (size == 0 || size > 8)
  {
    return 0;
  }
  return ((uint32_t)(value >> (8U - size))) << shift;
}

static uint32_t framebuffer_color(uint32_t rgb)
{
  uint8_t red = (uint8_t)(rgb >> 16);
  uint8_t green = (uint8_t)(rgb >> 8);
  uint8_t blue = (uint8_t)rgb;

  return color_component(red, framebuffer->red_mask_size,
          framebuffer->red_mask_shift) |
      color_component(green, framebuffer->green_mask_size,
          framebuffer->green_mask_shift) |
      color_component(blue, framebuffer->blue_mask_size,
          framebuffer->blue_mask_shift);
}

static volatile uint32_t *pixel_address(uint64_t x, uint64_t y)
{
  uint8_t *row = (uint8_t *)framebuffer->address + y * framebuffer->pitch;
  return (volatile uint32_t *)(row + x * sizeof(uint32_t));
}

static void fill_cell(uint64_t cell_x, uint64_t cell_y, uint32_t color)
{
  uint64_t start_x = cell_x * CELL_WIDTH;
  uint64_t start_y = cell_y * CELL_HEIGHT;
  for (uint64_t y = 0; y < CELL_HEIGHT; y++)
  {
    for (uint64_t x = 0; x < CELL_WIDTH; x++)
    {
      *pixel_address(start_x + x, start_y + y) = color;
    }
  }
}

static uint8_t glyph_row(char character, uint64_t row)
{
  if (character >= 'a' && character <= 'z')
  {
    character = (char)(character - 'a' + 'A');
  }
  if (character >= 'A' && character <= 'Z')
  {
    return font[(size_t)(character - 'A')][row];
  }
  if (character >= '0' && character <= '9')
  {
    return font[26U + (size_t)(character - '0')][row];
  }

  static const uint8_t punctuation[][FONT_HEIGHT] = {
    [0] = {0, 0, 0, 0, 0, 0, 0},
    [1] = {0, 0, 0, 0x1F, 0, 0, 0},
    [2] = {0, 0, 0, 0, 0, 0x0C, 0x0C},
    [3] = {0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0},
    [4] = {0x04, 0x04, 0x04, 0, 0x04, 0x04, 0x04},
    [5] = {0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04},
    [6] = {0x02, 0x04, 0x08, 0x10, 0, 0, 0},
    [7] = {0x0C, 0x12, 0x02, 0x04, 0x04, 0, 0x04},
    [8] = {0, 0, 0, 0, 0, 0x04, 0x08},
    [9] = {0x08, 0x04, 0, 0, 0, 0, 0},
    [10] = {0, 0x04, 0x0A, 0x11, 0, 0, 0},
    [11] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10},
    [12] = {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}
  };

  switch (character)
  {
    case ' ': return punctuation[0][row];
    case '-': return punctuation[1][row];
    case '.': return punctuation[2][row];
    case ':': return punctuation[3][row];
    case '_': return punctuation[4][row];
    case '!': return punctuation[5][row];
    case '/': return punctuation[6][row];
    case '?': return punctuation[7][row];
    case ',': return punctuation[8][row];
    case '\\': return punctuation[9][row];
    case '+': return punctuation[10][row];
    case '>': return punctuation[11][row];
    case '<': return punctuation[12][row];
    default: return 0x1F;
  }
}

static void draw_character(char character)
{
  uint64_t start_x = cursor_x * CELL_WIDTH + 1U;
  uint64_t start_y = cursor_y * CELL_HEIGHT + 2U;
  fill_cell(cursor_x, cursor_y, background);

  for (uint64_t row = 0; row < FONT_HEIGHT; row++)
  {
    uint8_t bits = glyph_row(character, row);
    for (uint64_t column = 0; column < FONT_WIDTH; column++)
    {
      if ((bits & (1U << (FONT_WIDTH - column - 1U))) == 0)
      {
        continue;
      }
      for (uint64_t y = 0; y < FONT_SCALE; y++)
      {
        for (uint64_t x = 0; x < FONT_SCALE; x++)
        {
          *pixel_address(start_x + column * FONT_SCALE + x,
              start_y + row * FONT_SCALE + y) = foreground;
        }
      }
    }
  }
}

static void scroll_console(void)
{
  uint64_t pixel_rows = framebuffer->height - CELL_HEIGHT;
  for (uint64_t y = 0; y < pixel_rows; y++)
  {
    volatile uint32_t *destination = pixel_address(0, y);
    volatile uint32_t *source = pixel_address(0, y + CELL_HEIGHT);
    for (uint64_t x = 0; x < framebuffer->width; x++)
    {
      destination[x] = source[x];
    }
  }

  for (uint64_t y = pixel_rows; y < framebuffer->height; y++)
  {
    volatile uint32_t *row = pixel_address(0, y);
    for (uint64_t x = 0; x < framebuffer->width; x++)
    {
      row[x] = background;
    }
  }
  cursor_y = framebuffer->height / CELL_HEIGHT - 1U;
}

static void advance_line(void)
{
  cursor_x = 0;
  cursor_y++;
  if (cursor_y >= framebuffer->height / CELL_HEIGHT)
  {
    scroll_console();
  }
}

bool vga_init(void)
{
  if (console_initialized)
  {
    return false;
  }

  framebuffer = boot_framebuffer();
  if (framebuffer == 0 || framebuffer->address == 0 ||
      framebuffer->width < CELL_WIDTH || framebuffer->height < CELL_HEIGHT ||
      framebuffer->bpp != 32 || framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
      framebuffer->pitch < framebuffer->width * sizeof(uint32_t))
  {
    return false;
  }

  foreground = framebuffer_color(palette[7]);
  background = framebuffer_color(palette[0]);
  console_initialized = true;
  vga_clear();
  return true;
}

bool vga_is_initialized(void)
{
  return console_initialized;
}

void vga_clear(void)
{
  if (!console_initialized)
  {
    return;
  }
  for (uint64_t y = 0; y < framebuffer->height; y++)
  {
    volatile uint32_t *row = pixel_address(0, y);
    for (uint64_t x = 0; x < framebuffer->width; x++)
    {
      row[x] = background;
    }
  }
  cursor_x = 0;
  cursor_y = 0;
}

void vga_set_color(uint8_t foreground_index, uint8_t background_index)
{
  if (foreground_index >= 16U || background_index >= 16U || framebuffer == 0)
  {
    return;
  }
  foreground = framebuffer_color(palette[foreground_index]);
  background = framebuffer_color(palette[background_index]);
}

void vga_write_char(char character)
{
  if (!console_initialized)
  {
    return;
  }

  uint64_t columns = framebuffer->width / CELL_WIDTH;
  if (character == '\n')
  {
    advance_line();
    return;
  }
  if (character == '\r')
  {
    cursor_x = 0;
    return;
  }
  if (character == '\b')
  {
    if (cursor_x > 0)
    {
      cursor_x--;
      draw_character(' ');
    }
    return;
  }
  if (character == '\t')
  {
    uint64_t spaces = 4U - cursor_x % 4U;
    while (spaces-- > 0)
    {
      vga_write_char(' ');
    }
    return;
  }

  draw_character(character);
  cursor_x++;
  if (cursor_x >= columns)
  {
    advance_line();
  }
}

void vga_write(const char *text)
{
  if (text == 0)
  {
    return;
  }
  while (*text != '\0')
  {
    vga_write_char(*text++);
  }
}