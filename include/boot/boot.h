#ifndef ETHOS_BOOT_H
#define ETHOS_BOOT_H

#include <stdbool.h>
#include <stdint.h>
#include <boot/limine/limine.h>

bool boot_init(void);
struct limine_memmap_response *boot_memory_map(void);
uint64_t boot_hhdm_offset(void);
struct limine_framebuffer *boot_framebuffer(void);
struct limine_module_response *boot_modules(void);

#endif
