# Kernel Architecture

Limine hands Ethos the boot information, memory map, framebuffer, and command modules. The kernel is a freestanding x86-64 C program; it brings up one processor and has no SMP path yet.

## Boot path

`kmain()` in `init/main.c` owns subsystem startup. It first captures boot data and enables serial diagnostics, then sets up the GDT/TSS, paging, IDT, PIC/PIT, and syscall MSRs. The scheduler and process manager follow, with RAMFS and the shell initialized after them. VGA initialization comes later; Ethos then runs its RAMFS checks, starts the scheduler, and enters the shell loop.

That order has a reason: process creation depends on both paging and the scheduler, while the shell needs the filesystem and input path. A failure during required subsystem setup stops the kernel and writes the reason to serial.

## Where code lives

| Work | Source |
| --- | --- |
| Limine requests and boot data | `limine/boot.c`, `include/boot/` |
| GDT, TSS, IDT | `arch/x86_64/`, `assembly/arch/x86_64/` |
| Page tables and address spaces | `mm/paging.c` |
| Timer scheduling | `sched/scheduler.c` |
| Process records and ELF loading | `proc/process.c`, `proc/exec.c` |
| System-call entry and dispatch | `assembly/arch/x86_64/syscall.S`, `arch/x86_64/syscall.c` |
| RAM filesystem | `fs/ramfs.c` |
| Framebuffer console and keyboard | `console/vga.c`, `input/keyboard.c` |
| Command loop | `shell/shell.c` |

## Timer and context switch

The PIT fires 100 times per second. Its interrupt handler builds a 22-word register frame; `scheduler_on_timer()` records the interrupted stack and either keeps the running task or selects a ready one. When the scheduler changes tasks, it also switches the page-table root and the kernel stack used by interrupts and system calls.

Kernel threads and user processes occupy the same scheduler table. A user process runs at ring 3 in its own address space. The [process model](process-model.md) describes task states and priority quanta in more detail.

## System-call path

User code executes `SYSCALL`. The assembly stub saves registers, moves to the running task's kernel stack, and calls the C dispatcher. The dispatcher reads the call number from `RAX`, takes its first argument from `RDI`, and returns its result in `RAX`; the instruction clobbers `RCX` and `R11`.

| `RAX` | Call | Effect |
| ---: | --- | --- |
| 0 | Exit | Mark the calling task terminated |
| 1 | Write character | Send one character to serial and VGA |
| 2 | Set priority | Change the caller's priority, from 0 to 3 |
| 3 | Get PID | Return the calling process ID |
| 4 | Input | Poll one keyboard character; return `-EAGAIN` when none is ready |

`include/ethos/syscall.h` defines the public ABI. User command builds link `user/lib/syscall.c`, which wraps these calls as `ethos_print()`, `ethos_input()`, `ethos_getpid()`, `ethos_set_priority()`, and `ethos_exit()`. Functions that return a signed value use negative error numbers; unknown calls return `-ENOSYS`, and calls made from a kernel task return `-EPERM`.

No syscall opens files, creates processes, or maps memory. The kernel shell owns filesystem operations and command loading; user applications should call the public API rather than kernel internals.

## Present limits

The kernel doesn't start secondary processors or mount a disk filesystem. The shell runs in kernel context, and its external programs come from static ELF64 modules supplied at boot. It waits for each program to finish before accepting another command.