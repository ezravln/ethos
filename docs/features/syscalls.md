# System Call Interface

User commands call the kernel through the API in `include/ethos/syscall.h`. Their implementations live in `user/lib/syscall.c`, which the build links into each command. Application code can call the Ethos functions instead of embedding `syscall` instructions or reaching into kernel code.

## Available calls

| Number | Public function | Result |
| ---: | --- | --- |
| 0 | `ethos_exit()` | Ends the calling process; it never returns. |
| 1 | `ethos_write_char(char)` | Returns 0 after writing one character to serial and VGA. |
| 2 | `ethos_set_priority(uint8_t)` | Returns 0 on success; accepts levels 0 through 3. |
| 3 | `ethos_getpid()` | Returns the calling process ID. |
| 4 | `ethos_input()` | Returns one keyboard byte, or a negative error if no key is ready. |

`ethos_print(const char *)` is a library helper built on `ethos_write_char()`. It returns the number of characters written, or the first negative error. It returns `-EINVAL` for a null pointer.

## Example

```c
#include <ethos/syscall.h>

__attribute__((noreturn)) void _start(void)
{
  int64_t pid = ethos_getpid();
  if (pid >= 0)
  {
    (void)ethos_print("Hello from Ethos.\n");
  }
  ethos_exit();
}
```

`ethos_input()` doesn't wait. A negative `-ETHOS_ERROR_TRY_AGAIN` result means the keyboard queue had no character; call it again later if polling suits the program. Other errors use the negative constants declared in the public header.

## Calling convention and privilege

The x86-64 syscall number goes in `RAX`; arguments go in `RDI`, `RSI`, `RDX`, `R10`, `R8`, and `R9`, in that order. The return value comes back in `RAX`. The instruction clobbers `RCX` and `R11`; the user library accounts for those registers.

`SYSCALL` enters ring 0 through the configured LSTAR entry. The assembly stub switches to the current task's kernel stack before dispatching the call. The dispatcher rejects calls unless the scheduler marks the current task as a user process. User code therefore can't use syscall 0 to terminate the kernel shell task.

Errors use negative values: `-EPERM` for a kernel task, `-EINVAL` for an invalid priority or null print string, `-ESRCH` when a process record can't be found, `-EAGAIN` when input isn't ready, and `-ENOSYS` for an unknown call. The numeric values appear as `ETHOS_ERROR_*` macros in `include/ethos/syscall.h`.

## Not available yet

The interface doesn't expose `open`, `read`, `write`, or `close` for files. It also has no process-spawn call, user memory allocator, or terminal line-input routine. RAMFS and command loading still run through kernel code; don't pass application pointers to the dispatcher because no user-memory copy or range-validation layer exists yet.