#include <stdint.h>
#include <stddef.h>

static const char message[] = "Hello from a binary command!\n";

__attribute__((noreturn)) void _start(void)
{
  for (size_t index = 0; index < sizeof(message) - 1U; index++)
  {
    uint64_t number = 1;
    uint64_t character = (uint8_t)message[index];
    __asm__ volatile (
      "syscall"
      : "+a"(number)
      : "D"(character)
      : "rcx", "r11", "memory"
    );
  }

  uint64_t number = 0;
  __asm__ volatile (
    "syscall"
    : "+a"(number)
    :
    : "rcx", "r11", "memory"
  );
  for (;;)
  {
    __asm__ volatile ("pause");
  }
}
