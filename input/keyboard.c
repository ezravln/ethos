#include <input/keyboard.h>
#include <stdint.h>

#define KEYBOARD_STATUS_PORT 0x64U
#define KEYBOARD_DATA_PORT 0x60U

static bool left_shift;
static bool right_shift;
static bool left_control;
static bool right_control;
static bool extended_scancode;
static bool initialized;

static inline uint8_t inb(uint16_t port)
{
  uint8_t value;
  __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

static char translate_scancode(uint8_t scancode, bool shifted)
{
  static const char normal[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x39] = ' '
  };
  static const char shifted_table[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"',
    [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>',
    [0x35] = '?'
  };

  char character = normal[scancode];
  if (character >= 'a' && character <= 'z' && shifted)
  {
    return (char)(character - 'a' + 'A');
  }
  if (shifted && shifted_table[scancode] != 0)
  {
    return shifted_table[scancode];
  }
  return character;
}

bool keyboard_init(void)
{
  if (initialized)
  {
    return false;
  }

  left_shift = false;
  right_shift = false;
  left_control = false;
  right_control = false;
  extended_scancode = false;
  initialized = true;
  return true;
}

bool keyboard_poll_event(struct keyboard_event *event)
{
  if (!initialized || event == 0 || (inb(KEYBOARD_STATUS_PORT) & 1U) == 0)
  {
    return false;
  }

  uint8_t scancode = inb(KEYBOARD_DATA_PORT);
  if (scancode == 0xE0U || scancode == 0xE1U)
  {
    extended_scancode = true;
    return false;
  }
  bool extended = extended_scancode;
  if (extended)
  {
    extended_scancode = false;
  }

  bool released = (scancode & 0x80U) != 0;
  uint8_t key = scancode & 0x7FU;
  if (key == 0x1DU)
  {
    if (extended)
    {
      right_control = !released;
    }
    else
    {
      left_control = !released;
    }
    return false;
  }
  if (key == 0x2AU)
  {
    left_shift = !released;
    return false;
  }
  if (key == 0x36U)
  {
    right_shift = !released;
    return false;
  }
  if (released)
  {
    return false;
  }

  event->key = KEYBOARD_KEY_CHARACTER;
  event->character = 0;
  event->modifiers = (left_shift || right_shift ? KEYBOARD_MOD_SHIFT : 0U) |
    (left_control || right_control ? KEYBOARD_MOD_CONTROL : 0U);
  if (extended)
  {
    switch (key)
    {
      case 0x48: event->key = KEYBOARD_KEY_UP; return true;
      case 0x50: event->key = KEYBOARD_KEY_DOWN; return true;
      case 0x4B: event->key = KEYBOARD_KEY_LEFT; return true;
      case 0x4D: event->key = KEYBOARD_KEY_RIGHT; return true;
      case 0x47: event->key = KEYBOARD_KEY_HOME; return true;
      case 0x4F: event->key = KEYBOARD_KEY_END; return true;
      case 0x53: event->key = KEYBOARD_KEY_DELETE; return true;
      default: return false;
    }
  }
  char character = translate_scancode(key, left_shift || right_shift);
  if ((left_control || right_control) && character >= 'a' && character <= 'z')
  {
    character = (char)(character - 'a' + 1);
  }
  event->character = character;
  return character != 0;
}

char keyboard_poll_char(void)
{
  struct keyboard_event event;
  if (!keyboard_poll_event(&event) || event.key != KEYBOARD_KEY_CHARACTER)
  {
    return 0;
  }
  return event.character;
}
