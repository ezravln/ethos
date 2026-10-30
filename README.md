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
cd Ethos
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

Planned work includes:

- GRUB boot support
- PIT Timer & Interrupt Handler
- Memory management
- Hardware drivers
- Process and thread management
- Storage and filesystem support
- User-space applications

The list may change as development progresses.

## Licensing

See [LICENSE](LICENSE) for the license information applicable to this repository.

Licensing for future editions and separately distributed components may be handled independently.

## Contributing

Contributions, bug reports, and suggestions are welcome. Please read [SUPPORT.md](SUPPORT.md) for information on getting help and reporting issues.