# Ethos Documentation

Start with the [kernel overview](architecture/overview.md) if you want to follow boot from Limine into the shell. The other pages answer narrower questions about executable loading, process scheduling, files, and development.

## Architecture

- [Kernel overview](architecture/overview.md) follows initialization and runtime paths.
- [Process model](architecture/process-model.md) explains how process records connect to scheduler tasks.

## Features

- [APX executables](features/apx-executables.md) describes the container header, packer, and ELF limits.
- [Process management](features/process-management.md) covers PID, lifecycle, and priority APIs.
- [RAMFS](features/ramfs.md) documents path rules, capacity, and write protection.
- [Shell](features/shell.md) lists commands and explains argument parsing.
- [Typer](features/typer.md) gives editor controls and save behavior.

## Development

[Build and run](development/build-and-run.md) covers the ISO workflow and adding a user command.

These pages describe code in this repository, not a promised feature roadmap. The project [README](../README.md) gives the shorter status summary.