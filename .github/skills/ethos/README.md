# EthOS skill

This skill contains repository-specific guidance for working in the EthOS kernel project.

## Purpose

Use this skill when making changes to the kernel, boot flow, memory manager, process scheduler, or architecture-specific x86_64 code.

## Quick context

- Project type: minimal x86_64 operating system
- Bootloader: Limine
- Main entry point: `init/main.c`
- Architecture code: `arch/x86_64/`
- Memory management: `mm/` and `include/mm/`
- Scheduler and processes: `sched/` and `proc/`
- Build system: `GNUmakefile` and `scripts/build.sh`

## Common workflow

1. Read the docs in `README.md` before changing behavior.
2. Prefer small, localized edits in the matching subsystem.
3. Keep architecture assumptions explicit and x86_64-aware.
4. Rebuild with the repository's make target after changes.

## Validation

Use the project build flow to verify correctness after any patch:

```bash
make
```

If the root build target is not appropriate for a small fix, use the closest relevant script or make target from the repository.
