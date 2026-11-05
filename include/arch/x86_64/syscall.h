#ifndef ETHOS_ARCH_X86_64_SYSCALL_H
#define ETHOS_ARCH_X86_64_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>
#include <ethos/syscall.h>

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
