#ifndef ETHOS_ARCH_X86_64_SYSCALL_H
#define ETHOS_ARCH_X86_64_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>

#define ETHOS_SYSCALL_EXIT 0U
#define ETHOS_SYSCALL_WRITE_CHAR 1U
#define ETHOS_SYSCALL_SET_PRIORITY 2U

static inline int64_t ethos_set_priority(uint8_t priority)
{
	uint64_t result = ETHOS_SYSCALL_SET_PRIORITY;
	__asm__ volatile (
		"syscall"
		: "+a"(result)
		: "D"((uint64_t)priority)
		: "rcx", "r11", "memory"
	);
	return (int64_t)result;
}

bool syscall_init(void);
void syscall_set_kernel_stack(uintptr_t stack_top);
uint64_t syscall_dispatch(
	uint64_t number,
	uint64_t argument1,
	uint64_t argument2,
	uint64_t argument3,
	uint64_t argument4,
	uint64_t argument5,
	uint64_t argument6
);
__attribute__((noreturn)) void enter_user_mode(uintptr_t entry, uintptr_t stack);

#endif
