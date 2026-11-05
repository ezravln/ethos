# Process Management

The process manager owns user-process records; the scheduler decides which task runs. Each user process currently has one task, and the shell waits for it to exit before it accepts another command. The [process model](../architecture/process-model.md) traces that path through address-space cleanup.

## IDs and state

The process table holds at most eight entries. Each receives a monotonically increasing PID that Ethos doesn't reuse before reboot. Scheduler tasks have separate task IDs; `kernel-main` also takes one of the scheduler's eight slots, which leaves fewer than eight slots for user programs when other kernel threads exist.

The scheduler reports `READY`, `RUNNING`, or `TERMINATED`. It doesn't report `BLOCKED` because Ethos has no blocked-task queue or wakeup mechanism.

## Priority

Priority values run from 0 to 3. A task gets `priority + 1` PIT ticks per turn; the PIT runs at 100 Hz, so level 3 holds the CPU for about 40 ms before yielding, while level 0 yields after about 10 ms. The scheduler then rotates through ready tasks instead of starving low-priority work. New tasks start at level 0.

Kernel code can call `scheduler_set_priority(task_id, priority)` or `process_set_priority(pid, priority)`. User code should call `ethos_set_priority()` from `<ethos/syscall.h>`; the API library handles the register convention and returns zero on success or a negative error number.

```c
int64_t result = ethos_set_priority(2);
if (result < 0) {
  /* The kernel rejected the request. */
}
```

Under the hood, syscall number 2 travels in `RAX`, the requested level in `RDI`, and the result comes back in `RAX`. Values above 3 fail. The user API also provides `ethos_getpid()`, `ethos_print()`, `ethos_write_char()`, `ethos_input()`, and `ethos_exit()`; see [System Call Interface](syscalls.md) for return values and calling rules.

## Lifetime and gaps

After a process exits or faults, the shell's wait path destroys its address space and frees the process-table slot. A user fault gets logged; a kernel fault still stops the machine.

User programs can't create processes or wait for children. The PCB has no parent PID or child list, and Ethos has no `fork`, `waitpid`, multiple threads per process, or priority inheritance.