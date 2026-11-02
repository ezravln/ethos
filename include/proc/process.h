#ifndef ETHOS_PROC_PROCESS_H
#define ETHOS_PROC_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ETHOS_PROCESS_NAME_MAX 31U

enum process_state {
	PROCESS_READY = 1,
	PROCESS_RUNNING,
	PROCESS_TERMINATED
};

struct process_info {
	uint64_t process_id;
	uint64_t task_id;
	char name[ETHOS_PROCESS_NAME_MAX + 1U];
	enum process_state state;
	uintptr_t entry;
	uintptr_t user_stack;
	uint64_t address_space_root;
};

bool process_init(void);
bool process_create(
		const char *name,
		uintptr_t entry,
		uintptr_t user_stack,
		uint64_t address_space_root,
		uint64_t *process_id
);
bool process_exec_command(const char *command);
bool process_wait(uint64_t process_id);
size_t process_count(void);
bool process_get_at(size_t index, struct process_info *info);

#endif
