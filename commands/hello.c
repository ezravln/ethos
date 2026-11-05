#include <ethos/syscall.h>

static const char message[] = "Hello from a binary command!\n";

__attribute__((noreturn)) void _start(void)
{
  if (ethos_getpid() <= 0)
  {
    (void)ethos_print("getpid failed\n");
  }
  if (ethos_set_priority(2) != 0)
  {
    (void)ethos_print("set_priority failed\n");
  }
  if (ethos_input() != -ETHOS_ERROR_TRY_AGAIN)
  {
    (void)ethos_print("input polling failed\n");
  }
  (void)ethos_print(message);
  ethos_exit();
}
