#include <proc/process.h>
#include <mm/paging.h>
#include <sched/scheduler.h>

#define ETHOS_MAX_PROCESSES 8U

struct process {
	struct process_info info;
	bool active;
};

static struct process processes[ETHOS_MAX_PROCESSES];
static uint64_t next_process_id = 1;
static bool process_initialized;

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

static bool copy_process_name(char *destination, const char *name)
{
	if (destination == 0 || name == 0)
	{
		return false;
	}

	size_t index = 0;
	while (name[index] != '\0')
	{
		unsigned char character = (unsigned char)name[index];
		if (index >= ETHOS_PROCESS_NAME_MAX || character < 0x21U || character > 0x7EU)
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

bool process_init(void)
{
	if (process_initialized || !paging_is_initialized() || !scheduler_is_initialized())
	{
		return false;
	}
	process_initialized = true;
	return true;
}

bool process_create(
		const char *name,
		uintptr_t entry,
		uintptr_t user_stack,
		uint64_t address_space_root,
		uint64_t *process_id
)
{
	char stored_name[ETHOS_PROCESS_NAME_MAX + 1U] = {0};
	if (!process_initialized || entry == 0 || user_stack == 0 ||
			address_space_root == 0 || next_process_id == UINT64_MAX ||
			!copy_process_name(stored_name, name))
	{
		return false;
	}

	uint64_t entry_flags;
	uint64_t stack_flags;
	if (!paging_get_mapping(address_space_root, entry, 0, &entry_flags) ||
			(entry_flags & PAGING_FLAG_USER) == 0 || user_stack < PAGING_PAGE_SIZE ||
			!paging_get_mapping(address_space_root, user_stack - 1U, 0, &stack_flags) ||
			(stack_flags & (PAGING_FLAG_USER | PAGING_FLAG_WRITE)) !=
					(PAGING_FLAG_USER | PAGING_FLAG_WRITE))
	{
		return false;
	}

	uint64_t flags = interrupt_disable();
	size_t slot = 0;
	while (slot < ETHOS_MAX_PROCESSES && processes[slot].active)
	{
		slot++;
	}
	if (slot == ETHOS_MAX_PROCESSES)
	{
		interrupt_restore(flags);
		return false;
	}

	uint64_t id = next_process_id++;
	processes[slot].info = (struct process_info) {
		.process_id = id,
		.task_id = 0,
		.name = {0},
		.state = PROCESS_READY,
		.priority = ETHOS_SCHEDULER_PRIORITY_DEFAULT,
		.entry = entry,
		.user_stack = user_stack,
		.address_space_root = address_space_root
	};
	for (size_t index = 0; index < sizeof(stored_name); index++)
	{
		processes[slot].info.name[index] = stored_name[index];
		if (stored_name[index] == '\0')
		{
			break;
		}
	}
	processes[slot].active = true;

	if (!thread_create_user(
			stored_name,
			id,
			entry,
			user_stack,
			address_space_root,
			&processes[slot].info.task_id))
	{
		processes[slot].active = false;
		interrupt_restore(flags);
		return false;
	}
	interrupt_restore(flags);
	if (process_id != 0)
	{
		*process_id = id;
	}
	return true;
}

size_t process_count(void)
{
	uint64_t flags = interrupt_disable();
	size_t count = 0;
	for (size_t index = 0; index < ETHOS_MAX_PROCESSES; index++)
	{
		if (processes[index].active)
		{
			count++;
		}
	}
	interrupt_restore(flags);
	return count;
}

bool process_wait(uint64_t process_id)
{
	if (!process_initialized || process_id == 0)
	{
		return false;
	}

	for (;;)
	{
		uint64_t flags = interrupt_disable();
		bool found = false;
		bool terminated = false;
		size_t process_slot = ETHOS_MAX_PROCESSES;
		for (size_t slot = 0; slot < ETHOS_MAX_PROCESSES; slot++)
		{
			if (!processes[slot].active ||
					processes[slot].info.process_id != process_id)
			{
				continue;
			}

			found = true;
			process_slot = slot;
			bool task_found = false;
			for (size_t task_index = 0; task_index < scheduler_task_count(); task_index++)
			{
				struct scheduler_task_info task;
				if (scheduler_task_get_at(task_index, &task) &&
						task.task_id == processes[slot].info.task_id)
				{
					task_found = true;
					terminated = task.state == SCHEDULER_TASK_TERMINATED;
					break;
				}
			}
			terminated = terminated || !task_found;
			break;
		}

		if (!found)
		{
			interrupt_restore(flags);
			return false;
		}
		if (terminated)
		{
			uint64_t address_space = processes[process_slot].info.address_space_root;
			if (!paging_destroy_address_space(address_space))
			{
				interrupt_restore(flags);
				return false;
			}
			processes[process_slot].active = false;
			interrupt_restore(flags);
			return true;
		}
		interrupt_restore(flags);
		__asm__ volatile ("sti; hlt" ::: "memory");
	}
}

bool process_set_priority(uint64_t process_id, uint8_t priority)
{
	if (!process_initialized || process_id == 0 ||
			priority > ETHOS_SCHEDULER_PRIORITY_MAX)
	{
		return false;
	}

	uint64_t flags = interrupt_disable();
	for (size_t slot = 0; slot < ETHOS_MAX_PROCESSES; slot++)
	{
		if (!processes[slot].active ||
				processes[slot].info.process_id != process_id)
		{
			continue;
		}

		if (!scheduler_set_priority(processes[slot].info.task_id, priority))
		{
			interrupt_restore(flags);
			return false;
		}
		processes[slot].info.priority = priority;
		interrupt_restore(flags);
		return true;
	}
	interrupt_restore(flags);
	return false;
}

bool process_get_at(size_t index, struct process_info *info)
{
	if (info == 0)
	{
		return false;
	}

	uint64_t flags = interrupt_disable();
	size_t active_index = 0;
	for (size_t slot = 0; slot < ETHOS_MAX_PROCESSES; slot++)
	{
		if (!processes[slot].active)
		{
			continue;
		}
		if (active_index++ != index)
		{
			continue;
		}

		*info = processes[slot].info;
		for (size_t task_index = 0; task_index < scheduler_task_count(); task_index++)
		{
			struct scheduler_task_info task;
			if (scheduler_task_get_at(task_index, &task) &&
					task.task_id == info->task_id)
			{
				info->priority = task.priority;
				info->state = task.state == SCHEDULER_TASK_TERMINATED
						? PROCESS_TERMINATED
						: (task.state == SCHEDULER_TASK_RUNNING
								? PROCESS_RUNNING
								: PROCESS_READY);
				break;
			}
		}
		interrupt_restore(flags);
		return true;
	}
	interrupt_restore(flags);
	return false;
}
