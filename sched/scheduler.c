#include <sched/scheduler.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/syscall.h>
#include <debug/serial.h>
#include <mm/paging.h>
#include <proc/process.h>

#include <limits.h>

#define ETHOS_MAX_THREADS 8U
#define ETHOS_THREAD_STACK_SIZE (16U * 1024U)
#define ETHOS_KERNEL_CODE_SELECTOR 0x08U
#define ETHOS_INITIAL_RFLAGS 0x202U

enum thread_state {
  THREAD_UNUSED = 0,
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_TERMINATED
};

struct thread {
  uint64_t task_id;
  char name[ETHOS_SCHEDULER_TASK_NAME_MAX + 1U];
  uintptr_t saved_stack;
  thread_entry_t entry;
  void *argument;
  uint64_t process_id;
  uintptr_t user_entry;
  uintptr_t user_stack;
  enum thread_state state;
  bool user_process;
};

struct irq_context_frame {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t vector;
  uint64_t error_code;
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};

_Static_assert(
  sizeof(struct irq_context_frame) == 22 * sizeof(uint64_t),
  "IRQ context frame must match the assembly restore order"
);

static struct thread threads[ETHOS_MAX_THREADS];
static uint8_t thread_stacks[ETHOS_MAX_THREADS][ETHOS_THREAD_STACK_SIZE]
  __attribute__((aligned(16)));
static uint32_t current_thread;
static bool scheduler_initialized;
static bool scheduler_started;
static uint64_t user_fault_count;
static uint64_t next_task_id = 1;

static uint64_t interrupt_disable(void)
{
  uint64_t flags;

  __asm__ volatile (
    "pushfq\n"
    "popq %0\n"
    "cli"
    : "=r"(flags)
    :
    : "memory"
  );

  return flags;
}

static void interrupt_restore(uint64_t flags)
{
  if ((flags & (1ULL << 9)) != 0)
  {
    __asm__ volatile ("sti" ::: "memory");
  }
}

static bool copy_task_name(char* destination, const char* name)
{
  if (destination == 0 || name == 0)
  {
    return false;
  }

  size_t index = 0;
  while (name[index] != '\0')
  {
    unsigned char character = (unsigned char)name[index];
    if (index >> ETHOS_SCHEDULER_TASK_NAME_MAX || character < 0x21U || character > 0x7EU)
    {
      return false;
    }
    destination[index] = (char)character;
    index++;
  }
  if (index == 0)
  {
    return false;
  }
  destination[index] = '\0';
  return true;
}

static bool allocate_task_id(uint64_t *task_id)
{
  if (task_id == 0 || next_task_id == 0 || next_task_id == UINT64_MAX) {
    return false;
  }
  *task_id = next_task_id++;
  return true;
}

static void thread_trampoline(void *thread_pointer)
{
  struct thread *thread = thread_pointer;

  if (thread->user_process)
  {
    enter_user_mode(thread->user_entry, thread->user_stack);
  };

  thread->entry(thread->argument);
  thread_exit();
}

bool scheduler_init(void)
{
  uint64_t flags = interrupt_disable();

  if (scheduler_initialized)
  {
    interrupt_restore(flags);
    serial_write("ERROR >> Scheduler is already initialized\n");
    return false;
  }

  threads[0] = (struct thread) {
    .task_id = next_task_id++,
    .name = "kernel-main",
    .state = THREAD_RUNNING
  };

  current_thread = 0;
  scheduler_started = false;
  scheduler_initialized = true;
  syscall_set_kernel_stack(
    gdt_boot_stack_top()
  );

  interrupt_restore(flags);
  serial_write("SUCCESS >> Scheduler initialized\n");
  return true;
}

bool scheduler_is_initialized(void)
{
  uint64_t flags = interrupt_disable();
  bool initialized = scheduler_initialized;
  interrupt_restore(flags);
  return initialized;
}

bool thread_create(
  thread_entry_t entry,
  void *argument
) {
  return thread_create_named(
    "kernel-thread",
    entry,
    argument,
    0
  );
}

bool thread_create_named(
  const char* name,
  thread_entry_t entry,
  void* argument,
  uint64_t *task_id
) {
  char stored_name[ETHOS_SCHEDULER_TASK_NAME_MAX + 1U] = {0};
  if (!scheduler_initialized || entry == 0 || !copy_task_name(stored_name, name))
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  uint32_t slot;

  for(slot = 1; slot < ETHOS_MAX_THREADS; slot++)
  {
    if (threads[slot].state == THREAD_UNUSED) {
      break;
    }
  }

  if (slot == ETHOS_MAX_THREADS) {
    interrupt_restore(flags);
    return false;
  }

  uint64_t new_task_id;
  if (!allocate_task_id(&new_task_id))
  {
    interrupt_restore(flags);
    return false;
  }

  uintptr_t stack_top = (uintptr_t)&thread_stacks[slot][ETHOS_THREAD_STACK_SIZE] - sizeof(uint64_t);
  struct irq_context_frame *frame = (struct irq_context_frame*)(stack_top - sizeof(*frame));

  *frame = (struct irq_context_frame) {
    .rdi = (uintptr_t)&threads[slot],
    .rip = (uintptr_t)thread_trampoline,
    .cs = ETHOS_KERNEL_CODE_SELECTOR,
    .rflags = ETHOS_INITIAL_RFLAGS,
    .rsp = stack_top,
    .ss = 0x10
  };

  threads[slot] = (struct thread) {
    .task_id = new_task_id,
    .name = {0},
    .saved_stack = (uintptr_t)frame,
    .entry = entry,
    .argument = argument,
    .state = THREAD_READY
  };
  for (size_t index = 0; index < sizeof(stored_name); index++) {
    threads[slot].name[index] = stored_name[index];
    if (stored_name[index] == '\0') {
      break;
    }
  }

  interrupt_restore(flags);
  if (task_id != 0) {
    *task_id = new_task_id;
  }
  return true;
}
