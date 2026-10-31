#include <debug/serial.h>
#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value)
{
  __asm__ volatile (
    "outb %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

static inline uint8_t inb(uint16_t port)
{
  uint8_t value;

  __asm__ volatile (
    "inb %1, %0"
    : "=a"(value)
    : "Nd"(port)
  );

  return value;
}

void serial_init(void)
{
  outb(COM1 + 1, 0x00);
  outb(COM1 + 3, 0x80);
  outb(COM1 + 0, 0x03);
  outb(COM1 + 1, 0x00);
  outb(COM1 + 3, 0x03);
  outb(COM1 + 2, 0xC7);
  outb(COM1 + 4, 0x0B);
}

void serial_write_char(char c)
{
  for (uint32_t timeout = 0; timeout < 1000000; timeout++)
  {
    if (inb(COM1 + 5) & 0x20)
    {
      outb(COM1, (uint8_t)c);
      return;
    }
  }
}

void serial_write(const char* str)
{
  if (str == 0)
  {
    return;
  }

  while (*str != '\0')
  {
    if (*str == '\n')
    {
      serial_write_char('\r');
    }

    serial_write_char(*str++);
  }
}

char serial_read_char(void)
{
    while ((inb(COM1 + 5) & 0x01) == 0)
    {
        __asm__ volatile ("pause");
    }

    return (char)inb(COM1);
}
