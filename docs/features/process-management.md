# Process Management

The process manager owns user-process records; the scheduler decides which task runs. Each user process currently has one task, and the shell waits for it to exit before it accepts another command. The [process model](../architecture/process-model.md) traces that path through address-space cleanup.

## IDs and state

The process table holds at most eight entries. Each receives a monotonically increasing PID that Ethos doesn't reuse before reboot. Scheduler tasks have separate task IDs; `kernel-main` also takes one of the scheduler's eight slots, which leaves fewer than eight slots for user programs when other kernel threads exist.

The scheduler reports `READY`, `RUNNING`, or `TERMINATED`. It doesn't report `BLOCKED` because Ethos has no blocked-task queue or wakeup mechanism.

## Priority

Priority values run from 0 to 3. A task gets `priority + 1` PIT ticks per turn; the PIT runs at 100 Hz, so level 3 holds the CPU for about 40 ms before yielding, while level 0 yields after about 10 ms. The scheduler then rotates through ready tasks instead of starving low-priority work. New tasks start at level 0.

Kernel code can call `scheduler_set_priority(task_id, priority)` or `process_set_priority(pid, priority)`. User code can change only its own priority through syscall 2. This C fragment shows the raw syscall ABI; it belongs inside a freestanding command and needs the usual `<stdint.h>` declaration for `uint64_t` and `int64_t`.

```c
uint64_t number = 2;
uint64_t priority = 2;
__asm__ volatile (
  "syscall"
  : "+a"(number)
  : "D"(priority)
  : "rcx", "r11", "memory"
);
if ((int64_t)number != 0) {
  /* The kernel rejected the request. */
}
```

The syscall number travels in `RAX`, the requested level in `RDI`, and the result comes back in `RAX`. Values above 3 fail.

## Lifetime and gaps

After a process exits or faults, the shell's wait path destroys its address space and frees the process-table slot. A user fault gets logged; a kernel fault still stops the machine.

User programs can't create processes or wait for children. The PCB has no parent PID or child list, and Ethos has no `fork`, `waitpid`, multiple threads per process, or priority inheritance.