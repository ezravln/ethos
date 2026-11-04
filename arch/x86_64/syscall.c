#include <arch/x86_64/syscall.h>
#include <arch/x86_64/gdt.h>
#include <console/vga.h>
#include <debug/serial.h>
#include <sched/scheduler.h>
#include <proc/process.h>

#define IA32_EFER 0xC0000080U
#define IA32_STAR 0xC0000081U
#define IA32_LSTAR 0xC0000082U
#define IA32_FMASK 0xC0000084U
#define EFER_SYSCALL_ENABLE (1ULL << 0)
#define SYSCALL_RFLAGS_MASK ((1ULL << 8) | (1ULL << 9) | (1ULL << 10))

uintptr_t ethos_syscall_kernel_stack;
extern void syscall_entry(void);

static uint64_t read_msr(uint32_t msr)
{
	uint32_t low;
	uint32_t high;
	__asm__ volatile (
		"rdmsr"
		: "=a"(low), "=d"(high)
		: "c"(msr)
	);
	return ((uint64_t)high << 32) | low;
}

static void write_msr(uint32_t msr, uint64_t value)
{
	__asm__ volatile (
		"wrmsr"
		:
		: "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32))
	);
}

bool syscall_init(void)
{
	write_msr(IA32_EFER, read_msr(IA32_EFER) | EFER_SYSCALL_ENABLE);
	write_msr(IA32_STAR, (0x10ULL << 48) | (0x08ULL << 32));
	write_msr(IA32_LSTAR, (uintptr_t)syscall_entry);
	write_msr(IA32_FMASK, SYSCALL_RFLAGS_MASK);
	serial_write("SUCCESS >> SYSCALL MSRs initialized\n");
	return true;
}

void syscall_set_kernel_stack(uintptr_t stack_top)
{
	ethos_syscall_kernel_stack = stack_top;
	gdt_set_kernel_stack(stack_top);
}

uint64_t syscall_dispatch(
		uint64_t number,
		uint64_t argument1,
		uint64_t argument2,
		uint64_t argument3,
		uint64_t argument4,
		uint64_t argument5,
		uint64_t argument6
)
{
	(void)argument2;
	(void)argument3;
	(void)argument4;
	(void)argument5;
	(void)argument6;

	if (number == ETHOS_SYSCALL_EXIT)
	{
		thread_exit();
	}
	if (number == ETHOS_SYSCALL_WRITE_CHAR)
	{
		char character = (char)argument1;
		serial_write_char(character);
		vga_write_char(character);
		return 0;
	}
	if (number == ETHOS_SYSCALL_SET_PRIORITY)
	{
		if (argument1 > ETHOS_SCHEDULER_PRIORITY_MAX)
		{
			return (uint64_t)-22;
		}
		return process_set_priority(scheduler_current_process_id(),
				(uint8_t)argument1) ? 0 : (uint64_t)-3;
	}
	return (uint64_t)-38;
}
