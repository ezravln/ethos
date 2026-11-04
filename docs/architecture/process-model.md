# Processes and Tasks

A process record tracks user-program state; a scheduler task tracks the CPU context that can run. Ethos currently gives each user process one task. Kernel threads have tasks too, but no PID.

## PCB and PID

`struct process_info` in `include/proc/process.h` holds the PID, task ID, name, state, priority, entry point, user stack, and address-space root. The process table has eight slots. PID allocation increases monotonically and doesn't reuse a number during the same boot.

The scheduler also has eight slots, but `kernel-main` occupies one from startup. Other kernel threads take slots as well, so the process table's limit doesn't promise eight simultaneous user programs.

## State and timer

The scheduler exposes three states:

| State | What it means |
| --- | --- |
| `READY` | The task can run when selected. |
| `RUNNING` | The task owns the CPU now. |
| `TERMINATED` | The task exited or faulted; the process manager can reclaim its resources. |

The PIT ticks at 100 Hz. With priority `p`, a task keeps the CPU for `p + 1` ticks before yielding to the next ready task; values range from 0 to 3. This weighted round-robin policy gives higher-priority tasks longer turns, while still rotating through ready tasks. The default is 0.

On a timer interrupt, the scheduler stores the interrupted stack. If the task's quantum has expired, it picks the next ready task, changes the address space when needed, and installs that task's kernel stack. The interrupt return restores the selected register frame.

## From command to exit

The shell matches a command against the Limine modules. The loader checks its APX header, parses the ELF image, maps loadable segments and an eight-page user stack, then copies the command arguments there. After allocating a PID and task, Ethos enters ring 3.

The shell waits while the command runs. A normal exit or a user exception marks the task terminated; `process_wait()` then destroys its address space and releases the process slot. A kernel exception still stops the whole system.

## Missing states and relationships

Ethos has no `BLOCKED` state or wake queue. Its wait path serves the shell's synchronous command execution; user programs can't wait on one another.

The PCB stores no parent PID or child list. User code also has no process-creation syscall, so `fork`, asynchronous `exec`, and `waitpid` don't exist. Multiple tasks per process and priority inheritance remain out of scope.