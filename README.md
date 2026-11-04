<div align="center">

# Ethos OS

**An independent operating system project.**

[![License](https://img.shields.io/badge/License-See%20LICENSE-blue?style=flat-square)](LICENSE)
[![Support](https://img.shields.io/badge/Support-See%20SUPPORT.md-brightgreen?style=flat-square)](SUPPORT.md)
[![Architecture](https://img.shields.io/badge/Architecture-x86--64-blueviolet?style=flat-square)](#technology)
[![Language](https://img.shields.io/badge/Language-C-00599C?style=flat-square&logo=c&logoColor=white)](#technology)

</div>

## About

Ethos is an operating system developed from scratch in C, targeting the x86-64 architecture.

The project focuses on kernel development, memory management, hardware support, and the components needed to build a usable operating system.

Ethos is currently under development and is not intended for everyday use.

## Technology

- **Language:** C
- **Target architecture:** x86-64
- **Bootloader:** Limine, with GRUB support planned
- **Emulator:** QEMU
- **Build system:** GNU Make

## Building and Running

Ethos is developed and tested on Linux. QEMU is used to run the system during development.

Clone the repository:

```bash
git clone https://github.com/ezravln/ethos.git
cd ethos
```

Build the project:

```bash
./scripts/build.sh
```

Run it in QEMU:

```bash
./scripts/run.sh
```

These commands may change as the build system develops. Check the project's build scripts and `GNUmakefile` if they do not match the current version.

## Development Status

Ethos is a work in progress. Development is focused on the kernel and the basic components required to run programs and manage hardware.

The current kernel includes:

- Limine boot metadata for the HHDM and physical memory map
- x86-64 GDT/TSS and IDT exception handling
- Remapped PIC with a 100 Hz PIT timer interrupt
- 4-level paging helpers with reusable pages and user address-space teardown
- Preemptive weighted round-robin scheduling with priorities 0-3 and ring-3 process support
- SYSCALL/SYSRET calls for exit, character output, and setting the current process priority
- Limine framebuffer text console and serial logging
- Fixed-capacity in-memory filesystem with nested directories, protected system paths, and file operations
- Interactive keyboard shell that resolves commands from Limine-loaded ELF modules

### Filesystem Layout

The initial RAMFS directory tree separates system tools, applications, libraries, and user data:

| Path | Purpose |
| --- | --- |
| `/bin` | Commands available system-wide |
| `/sbin` | System and kernel administration tools |
| `/usr/bin` | User-space applications loaded as ELF modules |
| `/boot` | Boot-critical kernel and Limine files; read-only in RAMFS |
| `/lib` | Shared libraries |
| `/lib/kernel` | Libraries reserved for kernel components |
| `/usr/lib` | Shared libraries for user applications |
| `/home/ethos` | Writable home directory |
| `/home/ethos/.local/bin` | Applications owned by the user |
| `/home/ethos/.local/lib` | Libraries owned by the user |
| `/home/ethos/Documents`, `/Downloads`, `/Projects` | Work and personal files |
| `/home/ethos/Media/{Music,Pictures,Videos}` | Media files |

The RAMFS currently stores up to 16 files of 4 KiB each, supports nested paths and directory creation, and is cleared at reboot. System-owned directories are protected from writes. The ISO staging tree mirrors these paths; its `/boot` contents are read-only because the image is booted from the ISO. The interactive shell includes `echo`, `ls`, `cat`, `pwd`, `clear`, `mkdir`, `touch`, `rm`, `help`, and `typer` built-ins. Paths are absolute or relative to `/`, since a current working directory is not implemented yet. It tokenizes command arguments, supports single- and double-quoted arguments and backslash escaping, then passes `argc`/`argv` to external APX commands. It is not GNU Bash: variable expansion, pipelines, redirection, scripting, and other shell syntax are not implemented. Each `commands/<name>.c` is built as a separate static ELF64 payload inside an Ethos APX v1 executable (`ETHOSAPX` header, x86-64 architecture) and automatically loaded by Limine from `/usr/bin/<name>.apx` under the command name; adding a command does not require editing a command table or Limine config. The `typer` editor lives in `apps/typer.c`; run `typer /home/ethos/Documents/notes.c`. It supports arrow-key navigation, Shift+arrows selection, Ctrl+A select all, Ctrl+C copy, Ctrl+X cut, Ctrl+V paste, Ctrl+S save, Ctrl+Q exit, Home/End line navigation, Ctrl+Home/End document navigation, and Backspace/Delete. It can edit files up to the RAMFS 4 KiB limit in writable directories. Kernel sources are discovered recursively by the build system, while generated artifacts, ISO staging, bootloader files, and user commands are excluded from kernel compilation. Persistent user files require a writable block-device filesystem and are still planned. Remaining work includes broader ELF support, writable block-device filesystems, broader device support, and user-space applications.

The list may change as development progresses.

## Licensing

See [LICENSE](LICENSE) for the license information applicable to this repository.

Licensing for future editions and separately distributed components may be handled independently.

## Contributing

Contributions, bug reports, and suggestions are welcome. Please read [SUPPORT.md](SUPPORT.md) for information on getting help and reporting issues.
