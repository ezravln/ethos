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

Put a freestanding C source in `commands/` and define `_start`. Include `<ethos/syscall.h>` to use the public user API; Make links `user/lib/syscall.c` into each command.

```c
#include <ethos/syscall.h>

__attribute__((noreturn)) void _start(void)
{
  (void)ethos_print("Hello from Ethos.\n");
  ethos_exit();
}
```

Make compiles each `commands/<name>.c` as a static ELF64 file and wraps it as `bin/commands/<name>.apx`. The ISO script registers the APX as a Limine module with the same name; type that name in the shell to run it. You don't need to edit a command table or Limine config by hand.

The [APX page](../features/apx-executables.md) documents loader limits. See the [system call guide](../features/syscalls.md) for public functions, ABI numbers, and error values.

## Outputs

Make writes `bin/ethos`, intermediate command ELFs, and APX files under `bin/commands/`. The ISO script stages boot files under `root/` and writes `build/ethos.iso`. Those paths hold generated artifacts; edit the sources and scripts instead.