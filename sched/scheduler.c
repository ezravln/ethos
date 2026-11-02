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
  uint64_t address_space_root;
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
static uint64_t kernel_address_space;

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
    if (index >= ETHOS_SCHEDULER_TASK_NAME_MAX || character < 0x21U || character > 0x7EU)
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
  }

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

  uint64_t main_task_id;
  if (!allocate_task_id(&main_task_id))
  {
    interrupt_restore(flags);
    return false;
  }
  threads[0] = (struct thread) {
    .task_id = main_task_id,
    .name = "kernel-main",
    .state = THREAD_RUNNING
  };
  current_thread = 0;
  scheduler_started = false;
  scheduler_initialized = true;
  kernel_address_space = paging_current_address_space();
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

static bool thread_create_internal(
  const char* name,
  thread_entry_t entry,
  void* argument,
  uint64_t *task_id,
  uint64_t process_id,
  uintptr_t user_entry,
  uintptr_t user_stack,
  uint64_t address_space_root,
  bool user_process
) {
  char stored_name[ETHOS_SCHEDULER_TASK_NAME_MAX + 1U] = {0};
  if (!scheduler_initialized ||
      (user_process ? (user_entry == 0 || user_stack == 0 || address_space_root == 0) : entry == 0) ||
      !copy_task_name(stored_name, name))
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
    .process_id = process_id,
    .address_space_root = address_space_root,
    .user_entry = user_entry,
    .user_stack = user_stack,
    .state = THREAD_READY,
    .user_process = user_process
  };
  for (size_t index = 0; index < sizeof(stored_name); index++) {
    threads[slot].name[index] = stored_name[index];
    if (stored_name[index] == '\0') {
      break;
    }
  }

  if (task_id != 0) {
    *task_id = new_task_id;
  }
  interrupt_restore(flags);
  return true;
}

bool thread_create_named(
  const char* name,
  thread_entry_t entry,
  void* argument,
  uint64_t *task_id
)
{
  return thread_create_internal(
    name, entry, argument, task_id, 0, 0, 0, 0, false
  );
}

bool thread_create_user(
  const char *name,
  uint64_t process_id,
  uintptr_t entry,
  uintptr_t user_stack,
  uint64_t address_space_root,
  uint64_t *task_id
)
{
  return thread_create_internal(
    name, 0, 0, task_id, process_id, entry, user_stack,
    address_space_root, true
  );
}

static uintptr_t choose_next_thread(uintptr_t interrupted_stack, bool save_current)
{
  struct thread *previous = &threads[current_thread];
  if (save_current)
  {
    previous->saved_stack = interrupted_stack;
    if (previous->state == THREAD_RUNNING)
    {
      previous->state = THREAD_READY;
    }
  }

  uint32_t next = current_thread;
  bool found = false;
  for (uint32_t offset = 1; offset <= ETHOS_MAX_THREADS; offset++)
  {
    uint32_t candidate = (current_thread + offset) % ETHOS_MAX_THREADS;
    if (threads[candidate].state == THREAD_READY)
    {
      next = candidate;
      found = true;
      break;
    }
  }

  if (!found)
  {
    if (previous->state != THREAD_TERMINATED)
    {
      previous->state = THREAD_RUNNING;
      return save_current ? interrupted_stack : previous->saved_stack;
    }
    for (;;)
    {
      __asm__ volatile ("sti; hlt" ::: "memory");
    }
  }

  current_thread = next;
  struct thread *selected = &threads[next];
  selected->state = THREAD_RUNNING;
  uint64_t target_address_space = selected->address_space_root != 0
      ? selected->address_space_root
      : kernel_address_space;
  if (target_address_space != paging_current_address_space())
  {
    paging_switch_address_space(target_address_space);
  }
  uintptr_t kernel_stack_top = next == 0
      ? gdt_boot_stack_top()
      : (uintptr_t)&thread_stacks[next][ETHOS_THREAD_STACK_SIZE];
  syscall_set_kernel_stack(kernel_stack_top);
  return selected->saved_stack;
}

bool scheduler_start(void)
{
  uint64_t flags = interrupt_disable();
  if (!scheduler_initialized || scheduler_started)
  {
    interrupt_restore(flags);
    return false;
  }

  scheduler_started = true;
  interrupt_restore(flags | (1ULL << 9));
  return true;
}

size_t scheduler_task_count(void)
{
  uint64_t flags = interrupt_disable();
  size_t count = 0;
  for (size_t index = 0; index < ETHOS_MAX_THREADS; index++)
  {
    if (threads[index].state != THREAD_UNUSED)
    {
      count++;
    }
  }
  interrupt_restore(flags);
  return count;
}

bool scheduler_task_get_at(size_t index, struct scheduler_task_info *info)
{
  if (info == 0)
  {
    return false;
  }

  uint64_t flags = interrupt_disable();
  size_t active_index = 0;
  for (size_t slot = 0; slot < ETHOS_MAX_THREADS; slot++)
  {
    struct thread *thread = &threads[slot];
    if (thread->state == THREAD_UNUSED)
    {
      continue;
    }
    if (active_index++ != index)
    {
      continue;
    }

    info->task_id = thread->task_id;
    for (size_t name_index = 0; name_index < sizeof(info->name); name_index++)
    {
      info->name[name_index] = thread->name[name_index];
    }
    info->state = thread->state == THREAD_RUNNING
        ? SCHEDULER_TASK_RUNNING
        : (thread->state == THREAD_TERMINATED
            ? SCHEDULER_TASK_TERMINATED
            : SCHEDULER_TASK_READY);
    info->process_id = thread->process_id;
    info->kernel_stack_size = ETHOS_THREAD_STACK_SIZE;
    info->user_process = thread->user_process;
    interrupt_restore(flags);
    return true;
  }

  interrupt_restore(flags);
  return false;
}

uintptr_t scheduler_on_timer(uintptr_t interrupted_stack)
{
  if (!scheduler_initialized || !scheduler_started)
  {
    return interrupted_stack;
  }
  return choose_next_thread(interrupted_stack, true);
}

uintptr_t scheduler_exit_current(void)
{
  if (!scheduler_initialized)
  {
    for (;;)
    {
      __asm__ volatile ("cli; hlt" ::: "memory");
    }
  }

  threads[current_thread].state = THREAD_TERMINATED;
  return choose_next_thread(0, false);
}

uint64_t scheduler_fault_current(uint64_t vector)
{
  if (scheduler_initialized && threads[current_thread].user_process)
  {
    user_fault_count++;
    threads[current_thread].state = THREAD_TERMINATED;
  }
  return vector;
}

uint64_t scheduler_user_fault_count(void)
{
  return user_fault_count;
}

uint64_t scheduler_current_address_space(void)
{
  return threads[current_thread].address_space_root != 0
      ? threads[current_thread].address_space_root
      : kernel_address_space;
}

bool scheduler_current_is_user(void)
{
  return scheduler_initialized && threads[current_thread].user_process;
}

uint64_t scheduler_current_process_id(void)
{
  return scheduler_initialized ? threads[current_thread].process_id : 0;
}

void thread_mark_current_terminated(void)
{
  if (scheduler_initialized)
  {
    threads[current_thread].state = THREAD_TERMINATED;
  }
}

__attribute__((noreturn)) void thread_exit(void)
{
  thread_mark_current_terminated();
  for (;;)
  {
    __asm__ volatile ("sti; hlt" ::: "memory");
  }
}
