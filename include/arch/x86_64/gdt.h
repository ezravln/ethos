#ifndef ETHOS_ARCH_X86_64_GDT_H
#define ETHOS_ARCH_X86_64_GDT_H

#include <stdbool.h>
#include <stdint.h>

bool gdt_init(void);
void gdt_set_kernel_stack(uintptr_t stack_top);
uintptr_t gdt_boot_stack_top(void);

#endif
