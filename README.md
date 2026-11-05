<div align="center">

# Ethos OS

**An independent operating system project.**

[![License](https://img.shields.io/badge/License-See%20LICENSE-blue?style=flat-square)](LICENSE)
[![Support](https://img.shields.io/badge/Support-See%20SUPPORT.md-brightgreen?style=flat-square)](SUPPORT.md)
[![Architecture](https://img.shields.io/badge/Architecture-x86--64-blueviolet?style=flat-square)](#technology)
[![Language](https://img.shields.io/badge/Language-C-00599C?style=flat-square&logo=c&logoColor=white)](#technology)

</div>

## About

Ethos is an x86-64 operating system written in C. Its kernel boots with Limine and runs in QEMU during development; the project is still experimental and isn't ready for daily use.

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
- User syscall API for exit, output, non-blocking keyboard input, PID lookup, and priority changes
- Kernel text-cell display server with four window slots, focus, z-order, and keyboard routing
- Polling QEMU E1000 Ethernet with DHCP, DNS, and ICMP ping; AX201 PCI detection works, but its Wi-Fi radio does not
- Limine framebuffer text console and serial logging
- Fixed-capacity in-memory filesystem with nested directories, protected system paths, and file operations
- Interactive keyboard shell that resolves commands from Limine-loaded ELF modules

## Documentation

Documentation is split by topic under [`docs/`](docs/README.md):

- [Architecture](docs/architecture/overview.md): boot flow, kernel subsystems, and process model
- [Features](docs/features/README.md): display server, Ethernet, AX201 Wi-Fi status, APX executables, process management, RAMFS, shell, Typer, and syscalls
- [Development](docs/development/build-and-run.md): prerequisites, build, QEMU, and adding commands

The implementation and its documentation may change as development progresses.

## Licensing

See [LICENSE](LICENSE) for the license information applicable to this repository.

Licensing for future editions and separately distributed components may be handled independently.

## Contributing

Contributions, bug reports, and suggestions are welcome. Please read [SUPPORT.md](SUPPORT.md) for information on getting help and reporting issues.
