# APX Executables

User commands travel inside APX files. The container adds a small header to a static ELF64 image; the kernel removes that header before handing the payload to its ELF loader.

## Header

The v1 header occupies 16 bytes. `struct apx_header` in `include/proc/apx.h` defines its packed layout.

| Field | Bytes | Current value |
| --- | ---: | --- |
| Magic | 8 | `ETHOSAPX` |
| Version | 2 | `1` |
| Header size | 2 | `16` |
| Architecture | 2 | `1`, x86-64 |
| Flags | 2 | `0` |

`tools/apxpack.c` writes the C struct directly, so the build host's byte order matters. Ethos targets x86-64 and builds on Linux, where this produces the little-endian header the loader expects. The loader reads the payload at `header_size`; it accepts larger headers and skips the extra bytes.

## How commands get there

Make compiles each `commands/<name>.c` into a static ELF, then runs `build/apxpack` to produce `bin/commands/<name>.apx`. The ISO script copies each APX under `/usr/bin/` and adds a Limine module name that matches the command. That lets the shell discover commands without a source-level command table.

The [command build guide](../development/build-and-run.md#adding-a-user-command) shows the required `_start` entry point.

## What the loader accepts

`proc/exec.c` checks the APX signature, version, header size, architecture, and flags before reading the ELF. The ELF loader accepts little-endian x86-64 ELF64 `ET_EXEC` images, maps `PT_LOAD` segments into a new user address space, and starts at an executable entry point. It allocates eight 4 KiB pages for the user stack.

Dynamic linking, relocations, shared libraries, ELF interpreters, other CPU architectures, and loading APX files from RAMFS aren't supported. Commands must arrive as Limine modules at boot.