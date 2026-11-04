#ifndef ETHOS_SCHED_SCHEDULER_H
#define ETHOS_SCHED_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ETHOS_SCHEDULER_TASK_NAME_MAX 31U
#define ETHOS_SCHEDULER_PRIORITY_MIN 0U
#define ETHOS_SCHEDULER_PRIORITY_MAX 3U
#define ETHOS_SCHEDULER_PRIORITY_DEFAULT 0U

enum scheduler_task_state {
  SCHEDULER_TASK_READY = 1,
  SCHEDULER_TASK_RUNNING,
  SCHEDULER_TASK_TERMINATED
};

struct scheduler_task_info {
  uint64_t task_id;
  char name[ETHOS_SCHEDULER_TASK_NAME_MAX + 1U];
  enum scheduler_task_state state;
  uint8_t priority;
  uint64_t process_id;
  uint64_t kernel_stack_size;
  bool user_process;
};

typedef void (*thread_entry_t)(void *argument);

bool scheduler_init(void);
bool scheduler_is_initialized(void);
bool thread_create(
  thread_entry_t entry,
  void *argument
);

bool thread_create_named(
  const char *name,
  thread_entry_t entry,
  void *argument,
  uint64_t *task_id
);

bool thread_create_user(
  const char *name,
  uint64_t process_id,
  uintptr_t entry,
  uintptr_t user_stack,
  uint64_t address_space_root,
  uint64_t *task_id
);
bool scheduler_set_priority(uint64_t task_id, uint8_t priority);

bool scheduler_start(void);
uint64_t scheduler_ticks(void);
size_t scheduler_task_count(void);
bool scheduler_task_get_at(
  size_t index,
  struct scheduler_task_info *info
);

uintptr_t scheduler_on_timer(uintptr_t interrupted_stack);
uintptr_t scheduler_exit_current(void);
uint64_t scheduler_fault_current(uint64_t vector);
uint64_t scheduler_user_fault_count(void);
uint64_t scheduler_current_address_space(void);
bool scheduler_current_is_user(void);
uint64_t scheduler_current_process_id(void);
__attribute__((noreturn)) void thread_exit(void);
void thread_mark_current_terminated(void);

#endif
