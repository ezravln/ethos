# Build and Run

Run these commands from the repository root on Linux. The build needs GNU Make, a compatible C compiler and linker, and `xorriso`; Limine's binaries and BIOS installer must already sit in `boot/limine/limine-binary/`. The build script checks for `xorriso`, but it doesn't download or compile Limine.

QEMU runs the image. Install `qemu-system-x86_64` separately if your system doesn't provide it.

## Build the ISO

```sh
./scripts/build.sh
```

The script invokes Make for the kernel and commands, copies the outputs and boot files into `root/`, then creates `build/ethos.iso`. If `bear` exists, the script runs Make through it to collect a compilation database.

Start QEMU with:

```sh
./scripts/run.sh
```

The VM gets 512 MiB of memory. QEMU attaches the generated ISO as its CD-ROM and sends serial output to the terminal. Press `Ctrl+C` there to stop the VM.

## Add a user command

Put a freestanding C source in `commands/` and define `_start`. The command below writes a short line, then exits through the syscall ABI:

```c
#include <stdint.h>

__attribute__((noreturn)) void _start(void)
{
  const char message[] = "Hello from Ethos.\n";
  for (uint64_t index = 0; index < sizeof(message) - 1U; index++)
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
```

Make compiles each `commands/<name>.c` as a static ELF64 file and wraps it as `bin/commands/<name>.apx`. The ISO script registers the APX as a Limine module with the same name; type that name in the shell to run it. You don't need to edit a command table or Limine config by hand.

The [APX page](../features/apx-executables.md) documents loader limits, while the [kernel overview](../architecture/overview.md#system-call-path) records the syscall numbers and registers.

## Outputs

Make writes `bin/ethos`, intermediate command ELFs, and APX files under `bin/commands/`. The ISO script stages boot files under `root/` and writes `build/ethos.iso`. Those paths hold generated artifacts; edit the sources and scripts instead.