---
name: ethos
description: Guidance for developing and debugging the EthOS kernel project.
---

# EthOS development skill

Use this skill when editing the EthOS kernel codebase. The project is a small x86_64 operating system with a Limine boot flow, custom memory paging, and basic process scheduling.

## Repo map

- `README.md`: project overview and expected behavior
- `init/main.c`: kernel entry point
- `arch/x86_64/`: CPU and interrupt setup
- `mm/`: page table and memory management logic
- `proc/`: process lifecycle and process structures
- `sched/`: scheduler logic
- `include/`: public kernel interfaces and declarations
- `boot/`: boot configuration files

## Working rules

- Keep code aligned with the x86_64 architecture and low-level kernel conventions.
- Preserve existing kernel initialization order and boot assumptions.
- Favor updates that are small, explicit, and local to the subsystem being changed.
- When extending memory or scheduling logic, check both the implementation and the matching header definitions.
- Avoid introducing abstractions or high-level patterns that hide kernel details unless the surrounding code already uses them.

## Build and validation

Use the project build as the minimum verification step after non-trivial changes:

```bash
make
```

For debugging-related tasks, inspect the relevant subsystem before editing, then validate the build and any runtime symptoms that the project supports.

## Typical change patterns

- Boot or loader issues: inspect `boot/`, `limine/`, and `arch/x86_64/`
- Segmentation or paging bugs: inspect `mm/` and `include/mm/`
- Scheduling or process problems: inspect `proc/`, `sched/`, and matching headers
- Interrupt or syscall issues: inspect `arch/x86_64/` and `include/arch/x86_64/`

## Safety guidance

This is kernel code, so correctness matters more than convenience. Prefer narrow, well-typed changes and verify buildability before considering a patch complete.
