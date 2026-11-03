#include <console/vga.h>
#include <boot/boot.h>
#include <stddef.h>

#define FONT_WIDTH 5U
#define FONT_HEIGHT 7U
#define FONT_SCALE_X 2U
#define FONT_SCALE_Y 2U
#define CELL_WIDTH 12U
#define CELL_HEIGHT 16U

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

static const uint8_t lowercase_font[26][FONT_HEIGHT] = {
  {0x00, 0x0E, 0x01, 0x0F, 0x11, 0x13, 0x0D},
  {0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E},
  {0x00, 0x0E, 0x10, 0x10, 0x10, 0x11, 0x0E},
  {0x01, 0x01, 0x0F, 0x11, 0x11, 0x13, 0x0D},
  {0x00, 0x0E, 0x11, 0x1F, 0x10, 0x11, 0x0E},
  {0x06, 0x09, 0x08, 0x1C, 0x08, 0x08, 0x08},
  {0x00, 0x0D, 0x13, 0x11, 0x0F, 0x01, 0x0E},
  {0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11},
  {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E},
  {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C},
  {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12},
  {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
  {0x00, 0x1A, 0x15, 0x15, 0x15, 0x15, 0x15},
  {0x00, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11},
  {0x00, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E},
  {0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10},
  {0x00, 0x0D, 0x13, 0x11, 0x0F, 0x01, 0x01},
  {0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x10},
  {0x00, 0x0F, 0x10, 0x0E, 0x01, 0x01, 0x1E},
  {0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06},
  {0x00, 0x11, 0x11, 0x11, 0x11, 0x13, 0x0D},
  {0x00, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
  {0x00, 0x11, 0x11, 0x11, 0x15, 0x15, 0x0A},
  {0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
  {0x00, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x0E},
  {0x00, 0x1F, 0x02, 0x04, 0x08, 0x10, 0x1F}
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
static bool cursor_visible;
static bool console_initialized;
static uint32_t utf8_codepoint;
static uint32_t utf8_minimum;
static uint8_t utf8_remaining;

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

static uint32_t latin1_base(uint32_t codepoint)
{
  if ((codepoint >= 0x00C0U && codepoint <= 0x00C5U) ||
      (codepoint >= 0x00E0U && codepoint <= 0x00E5U))
  {
    return codepoint < 0x00E0U ? 'A' : 'a';
  }
  if (codepoint == 0x00C7U || codepoint == 0x00E7U)
  {
    return codepoint == 0x00C7U ? 'C' : 'c';
  }
  if ((codepoint >= 0x00C8U && codepoint <= 0x00CBU) ||
      (codepoint >= 0x00E8U && codepoint <= 0x00EBU))
  {
    return codepoint < 0x00E0U ? 'E' : 'e';
  }
  if ((codepoint >= 0x00CCU && codepoint <= 0x00CFU) ||
      (codepoint >= 0x00ECU && codepoint <= 0x00EFU))
  {
    return codepoint < 0x00E0U ? 'I' : 'i';
  }
  if (codepoint == 0x00D1U || codepoint == 0x00F1U)
  {
    return codepoint == 0x00D1U ? 'N' : 'n';
  }
  if ((codepoint >= 0x00D2U && codepoint <= 0x00D6U) ||
      (codepoint >= 0x00F2U && codepoint <= 0x00F6U))
  {
    return codepoint < 0x00E0U ? 'O' : 'o';
  }
  if ((codepoint >= 0x00D9U && codepoint <= 0x00DCU) ||
      (codepoint >= 0x00F9U && codepoint <= 0x00FCU))
  {
    return codepoint < 0x00E0U ? 'U' : 'u';
  }
  if (codepoint == 0x00DDU || (codepoint >= 0x00FDU && codepoint <= 0x00FFU))
  {
    return codepoint == 0x00DDU ? 'Y' : 'y';
  }
  switch (codepoint)
  {
    case 0x00C6U: return 'A';
    case 0x00D0U: return 'D';
    case 0x00D8U: return 'O';
    case 0x00DEU: return 'P';
    case 0x00DFU: return 'b';
    case 0x00E6U: return 'a';
    case 0x00F0U: return 'd';
    case 0x00F8U: return 'o';
    case 0x00FEU: return 'p';
    case 0x00D7U: return '*';
    case 0x00F7U: return '/';
    default: return 0;
  }
}

static uint8_t latin1_accent(uint32_t codepoint)
{
  switch (codepoint)
  {
    case 0x00C0U: case 0x00E0U: case 0x00C8U: case 0x00E8U:
    case 0x00CCU: case 0x00ECU: case 0x00D2U: case 0x00F2U:
    case 0x00D9U: case 0x00F9U:
      return 1U;
    case 0x00C1U: case 0x00E1U: case 0x00C9U: case 0x00E9U:
    case 0x00CDU: case 0x00EDU: case 0x00D3U: case 0x00F3U:
    case 0x00DAU: case 0x00FAU: case 0x00DDU: case 0x00FDU:
      return 2U;
    case 0x00C2U: case 0x00E2U: case 0x00CAU: case 0x00EAU:
    case 0x00CEU: case 0x00EEU: case 0x00D4U: case 0x00F4U:
    case 0x00DBU: case 0x00FBU:
      return 3U;
    case 0x00C3U: case 0x00E3U: case 0x00D1U: case 0x00F1U:
    case 0x00D5U: case 0x00F5U:
      return 4U;
    case 0x00C4U: case 0x00E4U: case 0x00CBU: case 0x00EBU:
    case 0x00CFU: case 0x00EFU: case 0x00D6U: case 0x00F6U:
    case 0x00DCU: case 0x00FCU: case 0x00FFU:
      return 5U;
    case 0x00C5U: case 0x00E5U:
      return 6U;
    case 0x00C7U: case 0x00E7U:
      return 7U;
    default:
      return 0U;
  }
}

static uint8_t accent_row(uint8_t accent, uint64_t row)
{
  static const uint8_t marks[7][FONT_HEIGHT] = {
    {0x08, 0, 0, 0, 0, 0, 0},
    {0x02, 0, 0, 0, 0, 0, 0},
    {0x0A, 0, 0, 0, 0, 0, 0},
    {0x0A, 0, 0, 0, 0, 0, 0},
    {0x11, 0, 0, 0, 0, 0, 0},
    {0x04, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0x08}
  };
  if (accent == 0U || accent > 7U || row >= FONT_HEIGHT)
  {
    return 0;
  }
  return marks[accent - 1U][row];
}

static uint8_t glyph_row(uint32_t character, uint64_t row)
{
  uint8_t accent = latin1_accent(character);
  if (accent != 0U)
  {
    uint32_t base = latin1_base(character);
    if (base >= 'a' && base <= 'z')
    {
      uint8_t bits = lowercase_font[(size_t)(base - 'a')][row];
      return bits | accent_row(accent, row);
    }
    if (row == 0U)
    {
      return accent_row(accent, row);
    }
    static const uint8_t upper_rows[6] = {0, 1, 2, 3, 4, 6};
    uint8_t bits = font[(size_t)(base - 'A')][upper_rows[row - 1U]];
    return bits | accent_row(accent, row);
  }
  uint32_t base = latin1_base(character);
  if (base != 0U)
  {
    character = base;
  }
  if (character >= 'A' && character <= 'Z')
  {
    return font[(size_t)(character - 'A')][row];
  }
  if (character >= 'a' && character <= 'z')
  {
    return lowercase_font[(size_t)(character - 'a')][row];
  }
  if (character >= '0' && character <= '9')
  {
    return font[26U + (size_t)(character - '0')][row];
  }

  static const uint8_t punctuation[][FONT_HEIGHT] = {
    [0] = {0, 0, 0, 0, 0, 0, 0},
    [1] = {0, 0, 0, 0x0E, 0, 0, 0},
    [2] = {0, 0, 0, 0, 0, 0x04, 0x04},
    [3] = {0, 0x04, 0x04, 0, 0, 0x04, 0x04},
    [4] = {0, 0, 0, 0, 0, 0, 0x1F},
    [5] = {0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04},
    [6] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},
    [7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0, 0x04},
    [8] = {0, 0, 0, 0, 0, 0x04, 0x08},
    [9] = {0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01},
    [10] = {0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0},
    [11] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10},
    [12] = {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01},
    [13] = {0, 0x04, 0x04, 0, 0x04, 0x04, 0x08},
    [14] = {0x04, 0x04, 0x02, 0, 0, 0, 0},
    [15] = {0x0A, 0x0A, 0x05, 0, 0, 0, 0},
    [16] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    [17] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    [18] = {0, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0},
    [19] = {0, 0, 0x1F, 0, 0x1F, 0, 0},
    [20] = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13},
    [21] = {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D},
    [22] = {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},
    [23] = {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0F},
    [24] = {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},
    [25] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},
    [26] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},
    [27] = {0x04, 0x0A, 0x11, 0, 0, 0, 0},
    [28] = {0, 0, 0x09, 0x16, 0, 0, 0},
    [29] = {0x06, 0x04, 0x04, 0x18, 0x04, 0x04, 0x06},
    [30] = {0x0C, 0x04, 0x04, 0x03, 0x04, 0x04, 0x0C},
    [32] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    [33] = {0x08, 0x04, 0, 0, 0, 0, 0}
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
    case ';': return punctuation[13][row];
    case '\'': return punctuation[14][row];
    case '"': return punctuation[15][row];
    case '(': return punctuation[16][row];
    case ')': return punctuation[17][row];
    case '*': return punctuation[18][row];
    case '=': return punctuation[19][row];
    case '%': return punctuation[20][row];
    case '&': return punctuation[21][row];
    case '#': return punctuation[22][row];
    case '@': return punctuation[23][row];
    case '$': return punctuation[24][row];
    case '[': return punctuation[25][row];
    case ']': return punctuation[26][row];
    case '^': return punctuation[27][row];
    case '~': return punctuation[28][row];
    case '{': return punctuation[29][row];
    case '}': return punctuation[30][row];
    case '|': return punctuation[32][row];
    case '`': return punctuation[33][row];
    default: return punctuation[7][row];
  }
}

static void draw_character(uint32_t character)
{
  uint64_t start_x = cursor_x * CELL_WIDTH + 1U;
  uint64_t start_y = cursor_y * CELL_HEIGHT + 1U;
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
      for (uint64_t y = 0; y < FONT_SCALE_Y; y++)
      {
        for (uint64_t x = 0; x < FONT_SCALE_X; x++)
        {
          *pixel_address(start_x + column * FONT_SCALE_X + x,
              start_y + row * FONT_SCALE_Y + y) = foreground;
        }
      }
    }
  }
}

static void draw_cursor(void)
{
  uint64_t start_x = cursor_x * CELL_WIDTH + 1U;
  uint64_t start_y = cursor_y * CELL_HEIGHT + CELL_HEIGHT - 2U;
  for (uint64_t y = 0; y < 2U; y++)
  {
    for (uint64_t x = 0; x < CELL_WIDTH - 2U; x++)
    {
      *pixel_address(start_x + x, start_y + y) = foreground;
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
  cursor_visible = false;
}

void vga_set_color(uint8_t foreground_index, uint8_t background_index)
{
  if (foreground_index >= 16U || background_index >= 16U || framebuffer == 0)
  {
    return;
  }
  foreground = framebuffer_color(palette[foreground_index]);
  background = framebuffer_color(palette[background_index]);
  if (cursor_visible)
  {
    fill_cell(cursor_x, cursor_y, background);
    draw_cursor();
  }
}

void vga_set_cursor_visible(bool visible)
{
  if (!console_initialized || cursor_visible == visible)
  {
    return;
  }
  cursor_visible = visible;
  if (cursor_visible)
  {
    draw_cursor();
  }
  else
  {
    fill_cell(cursor_x, cursor_y, background);
  }
}

static void write_codepoint(uint32_t character)
{
  if (!console_initialized)
  {
    return;
  }

  uint64_t columns = framebuffer->width / CELL_WIDTH;
  if (cursor_visible)
  {
    fill_cell(cursor_x, cursor_y, background);
  }
  if (character == '\n')
  {
    advance_line();
    if (cursor_visible)
    {
      draw_cursor();
    }
    return;
  }
  if (character == '\r')
  {
    cursor_x = 0;
    if (cursor_visible)
    {
      draw_cursor();
    }
    return;
  }
  if (character == '\b')
  {
    if (cursor_x > 0)
    {
      cursor_x--;
      draw_character(' ');
    }
    if (cursor_visible)
    {
      draw_cursor();
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
  if (cursor_visible)
  {
    draw_cursor();
  }
}

void vga_write_char(char character)
{
  uint8_t byte = (uint8_t)character;
  if (utf8_remaining != 0U)
  {
    if ((byte & 0xC0U) == 0x80U)
    {
      utf8_codepoint = (utf8_codepoint << 6) | (byte & 0x3FU);
      utf8_remaining--;
      if (utf8_remaining == 0U)
      {
        uint32_t codepoint = utf8_codepoint;
        if (codepoint < utf8_minimum || codepoint > 0x10FFFFU ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU))
        {
          codepoint = 0xFFFDU;
        }
        write_codepoint(codepoint);
      }
      return;
    }
    utf8_remaining = 0;
    write_codepoint(0xFFFDU);
  }

  if (byte < 0x80U)
  {
    write_codepoint(byte);
  }
  else if (byte >= 0xC2U && byte <= 0xDFU)
  {
    utf8_codepoint = byte & 0x1FU;
    utf8_minimum = 0x80U;
    utf8_remaining = 1U;
  }
  else if (byte >= 0xE0U && byte <= 0xEFU)
  {
    utf8_codepoint = byte & 0x0FU;
    utf8_minimum = 0x800U;
    utf8_remaining = 2U;
  }
  else if (byte >= 0xF0U && byte <= 0xF4U)
  {
    utf8_codepoint = byte & 0x07U;
    utf8_minimum = 0x10000U;
    utf8_remaining = 3U;
  }
  else
  {
    write_codepoint(0xFFFDU);
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