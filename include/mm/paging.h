#ifndef ETHOS_MM_PAGING_H
#define ETHOS_MM_PAGING_H

#include <stdbool.h>
#include <stdint.h>

#define PAGING_PAGE_SIZE 4096U
#define PAGING_FLAG_WRITE (1ULL << 1)
#define PAGING_FLAG_USER (1ULL << 2)
#define PAGING_FLAG_WRITE_THROUGH (1ULL << 3)
#define PAGING_FLAG_CACHE_DISABLE (1ULL << 4)
#define PAGING_FLAG_NO_EXECUTE (1ULL << 63)

bool paging_init(void);
bool paging_is_initialized(void);
void *paging_alloc_page(void);
uint64_t paging_create_address_space(void);
uint64_t paging_current_address_space(void);
bool paging_switch_address_space(uint64_t physical_root);
bool paging_map_page(
	uint64_t physical_root,
	uintptr_t virtual_address,
	uint64_t physical_address,
	uint64_t flags
);
bool paging_unmap_page(uint64_t physical_root, uintptr_t virtual_address);
bool paging_get_mapping(
	uint64_t physical_root,
	uintptr_t virtual_address,
	uint64_t *physical_address,
	uint64_t *flags
);

#endif
