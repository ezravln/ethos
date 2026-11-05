#include <ethos/syscall.h>
#include <stddef.h>

static int64_t invoke_syscall(uint64_t number, uint64_t argument1)
{
  uint64_t result = number;
  __asm__ volatile (
    "syscall"
    : "+a"(result)
    : "D"(argument1)
    : "rcx", "r11", "memory"
  );
  return (int64_t)result;
}

int64_t ethos_write_char(char character)
{
  return invoke_syscall(ETHOS_SYSCALL_WRITE_CHAR,
      (uint8_t)character);
}

int64_t ethos_print(const char *text)
{
  if (text == 0)
  {
    return -ETHOS_ERROR_INVALID_ARGUMENT;
  }

  int64_t written = 0;
  while (text[written] != '\0')
  {
    int64_t result = ethos_write_char(text[written]);
    if (result < 0)
    {
      return result;
    }
    written++;
  }
  return written;
}

int64_t ethos_input(void)
{
  return invoke_syscall(ETHOS_SYSCALL_INPUT, 0);
}

int64_t ethos_getpid(void)
{
  return invoke_syscall(ETHOS_SYSCALL_GETPID, 0);
}

int64_t ethos_set_priority(uint8_t priority)
{
  return invoke_syscall(ETHOS_SYSCALL_SET_PRIORITY, priority);
}

__attribute__((noreturn)) void ethos_exit(void)
{
  (void)invoke_syscall(ETHOS_SYSCALL_EXIT, 0);
  for (;;)
  {
    __asm__ volatile ("pause");
  }
}