#include <mm/paging.h>
#include <boot/boot.h>
#include <lib/memory.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_LARGE (1ULL << 7)
#define PAGE_ADDRESS 0x000FFFFFFFFFF000ULL
#define PAGE_TABLE_ENTRIES 512U
#define LOW_MEMORY_LIMIT 0x100000ULL
#define PAGE_ALLOWED_FLAGS (PAGING_FLAG_WRITE | PAGING_FLAG_USER | \
	PAGING_FLAG_WRITE_THROUGH | PAGING_FLAG_CACHE_DISABLE | \
	PAGING_FLAG_NO_EXECUTE)

static struct limine_memmap_response *memory_map;
static uint64_t hhdm_offset;
static size_t allocator_entry;
static uint64_t allocator_cursor;
static uint64_t free_page_head;
static bool paging_initialized;
static bool nx_supported;

static uint64_t *physical_to_virtual(uint64_t physical_address)
{
	return (uint64_t *)(uintptr_t)(hhdm_offset + physical_address);
}

static bool is_canonical(uintptr_t address)
{
	uint64_t upper = (uint64_t)address >> 47;
	return upper == 0 || upper == 0x1FFFF;
}

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

static bool allocate_physical_page(uint64_t *physical_address)
{
	while (allocator_entry < memory_map->entry_count)
	{
		struct limine_memmap_entry *entry = memory_map->entries[allocator_entry];
		uint64_t entry_end;

		if (entry == 0 || entry->type != LIMINE_MEMMAP_USABLE ||
				entry->length > UINT64_MAX - entry->base)
		{
			allocator_entry++;
			allocator_cursor = 0;
			continue;
		}

		entry_end = entry->base + entry->length;
		if (allocator_cursor == 0)
		{
			uint64_t allocation_start = entry->base < LOW_MEMORY_LIMIT
					? LOW_MEMORY_LIMIT
					: entry->base;
			if (allocation_start > UINT64_MAX - (PAGING_PAGE_SIZE - 1U))
			{
				allocator_entry++;
				continue;
			}
			allocator_cursor = (allocation_start + PAGING_PAGE_SIZE - 1U) &
					~(uint64_t)(PAGING_PAGE_SIZE - 1U);
		}

		if (allocator_cursor <= entry_end &&
				entry_end - allocator_cursor >= PAGING_PAGE_SIZE)
		{
			*physical_address = allocator_cursor;
			allocator_cursor += PAGING_PAGE_SIZE;
			return true;
		}

		allocator_entry++;
		allocator_cursor = 0;
	}

	return false;
}

bool paging_init(void)
{
	if (paging_initialized)
	{
		return false;
	}

	memory_map = boot_memory_map();
	hhdm_offset = boot_hhdm_offset();
	if (memory_map == 0 || memory_map->entries == 0 || hhdm_offset == 0)
	{
		return false;
	}

	uint32_t eax = 0x80000000U;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	__asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
	if (eax >= 0x80000001U)
	{
		eax = 0x80000001U;
		__asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
		nx_supported = (edx & (1U << 20)) != 0;
	}
	if (nx_supported)
	{
		uint32_t efer_low;
		uint32_t efer_high;
		__asm__ volatile (
			"rdmsr"
			: "=a"(efer_low), "=d"(efer_high)
			: "c"(0xC0000080U)
		);
		efer_low |= 1U << 11;
		__asm__ volatile (
			"wrmsr"
			:
			: "c"(0xC0000080U), "a"(efer_low), "d"(efer_high)
		);
	}

	paging_initialized = true;
	return true;
}

bool paging_is_initialized(void)
{
	return paging_initialized;
}

void *paging_alloc_page(void)
{
	uint64_t flags = interrupt_disable();
	uint64_t physical_address = free_page_head;
	if (!paging_initialized)
	{
		interrupt_restore(flags);
		return 0;
	}
	if (physical_address != 0)
	{
		uint64_t *page_words = physical_to_virtual(physical_address);
		free_page_head = page_words[0];
	}
	else if (!allocate_physical_page(&physical_address))
	{
		interrupt_restore(flags);
		return 0;
	}
	if (physical_address > UINT64_MAX - hhdm_offset)
	{
		interrupt_restore(flags);
		return 0;
	}

	void *page = (void *)(uintptr_t)(hhdm_offset + physical_address);
	memset(page, 0, PAGING_PAGE_SIZE);
	interrupt_restore(flags);
	return page;
}

bool paging_free_page(void *page)
{
	if (!paging_initialized || page == 0)
	{
		return false;
	}
	uintptr_t virtual_address = (uintptr_t)page;
	if (virtual_address < hhdm_offset ||
			(virtual_address - hhdm_offset) % PAGING_PAGE_SIZE != 0)
	{
		return false;
	}
	uint64_t physical_address = virtual_address - hhdm_offset;
	if (physical_address > PAGE_ADDRESS)
	{
		return false;
	}

	uint64_t flags = interrupt_disable();
	for (uint64_t free_page = free_page_head; free_page != 0;
			free_page = physical_to_virtual(free_page)[0])
	{
		if (free_page == physical_address)
		{
			interrupt_restore(flags);
			return false;
		}
	}

	bool usable = false;
	for (size_t index = 0; index < memory_map->entry_count; index++)
	{
		struct limine_memmap_entry *entry = memory_map->entries[index];
		if (entry != 0 && entry->type == LIMINE_MEMMAP_USABLE &&
				entry->length <= UINT64_MAX - entry->base &&
				physical_address >= entry->base &&
				physical_address - entry->base <= entry->length &&
				entry->length - (physical_address - entry->base) >= PAGING_PAGE_SIZE)
		{
			usable = true;
			break;
		}
	}
	if (!usable)
	{
		interrupt_restore(flags);
		return false;
	}

	uint64_t *page_words = page;
	page_words[0] = free_page_head;
	free_page_head = physical_address;
	interrupt_restore(flags);
	return true;
}

uint64_t paging_current_address_space(void)
{
	uint64_t root;
	__asm__ volatile ("mov %%cr3, %0" : "=r"(root));
	return root & PAGE_ADDRESS;
}

uint64_t paging_create_address_space(void)
{
	uint64_t *new_root = paging_alloc_page();
	if (new_root == 0)
	{
		return 0;
	}

	uint64_t *kernel_root = physical_to_virtual(paging_current_address_space());
	for (size_t index = 256; index < PAGE_TABLE_ENTRIES; index++)
	{
		new_root[index] = kernel_root[index];
	}

	return (uint64_t)(uintptr_t)new_root - hhdm_offset;
}

static bool destroy_user_table(uint64_t table_physical, unsigned int level)
{
	uint64_t *table = physical_to_virtual(table_physical);
	for (size_t index = 0; index < PAGE_TABLE_ENTRIES; index++)
	{
		uint64_t entry = table[index];
		if ((entry & PAGE_PRESENT) == 0)
		{
			continue;
		}
		if (level > 1U)
		{
			if ((entry & PAGE_LARGE) != 0 ||
					!destroy_user_table(entry & PAGE_ADDRESS, level - 1U))
			{
				return false;
			}
			void *child = (void *)(uintptr_t)(hhdm_offset + (entry & PAGE_ADDRESS));
			if (!paging_free_page(child))
			{
				return false;
			}
		}
		else
		{
			void *backing_page = (void *)(uintptr_t)(hhdm_offset +
					(entry & PAGE_ADDRESS));
			if (!paging_free_page(backing_page))
			{
				return false;
			}
		}
		table[index] = 0;
	}
	return true;
}

bool paging_destroy_address_space(uint64_t physical_root)
{
	if (!paging_initialized || physical_root == 0 ||
			physical_root == paging_current_address_space() ||
			(physical_root & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			physical_root > PAGE_ADDRESS)
	{
		return false;
	}

	uint64_t *root = physical_to_virtual(physical_root);
	for (size_t index = 0; index < PAGE_TABLE_ENTRIES / 2U; index++)
	{
		uint64_t entry = root[index];
		if ((entry & PAGE_PRESENT) == 0)
		{
			continue;
		}
		if ((entry & PAGE_LARGE) != 0 ||
				!destroy_user_table(entry & PAGE_ADDRESS, 3U))
		{
			return false;
		}
		void *child = (void *)(uintptr_t)(hhdm_offset + (entry & PAGE_ADDRESS));
		if (!paging_free_page(child))
		{
			return false;
		}
		root[index] = 0;
	}
	return paging_free_page(root);
}

bool paging_switch_address_space(uint64_t physical_root)
{
	if (!paging_initialized || physical_root == 0 ||
			(physical_root & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			physical_root > PAGE_ADDRESS)
	{
		return false;
	}

	__asm__ volatile ("mov %0, %%cr3" : : "r"(physical_root) : "memory");
	return true;
}

static uint64_t *walk_to_leaf(
		uint64_t physical_root,
		uintptr_t virtual_address,
		uint64_t flags,
		bool create
)
{
	uint64_t indices[4] = {
		((uint64_t)virtual_address >> 39) & 0x1FFU,
		((uint64_t)virtual_address >> 30) & 0x1FFU,
		((uint64_t)virtual_address >> 21) & 0x1FFU,
		((uint64_t)virtual_address >> 12) & 0x1FFU
	};
	uint64_t *table = physical_to_virtual(physical_root);

	for (size_t level = 0; level < 3; level++)
	{
		uint64_t *entry = &table[indices[level]];
		if ((*entry & PAGE_PRESENT) == 0)
		{
			if (!create)
			{
				return 0;
			}

			uint64_t *next_table = paging_alloc_page();
			if (next_table == 0)
			{
				return 0;
			}
			uint64_t next_physical = (uint64_t)(uintptr_t)next_table - hhdm_offset;
			*entry = next_physical | PAGE_PRESENT | PAGING_FLAG_WRITE |
					(flags & PAGING_FLAG_USER);
		}
		else
		{
			if ((*entry & PAGE_LARGE) != 0)
			{
				return 0;
			}
			if ((flags & PAGING_FLAG_USER) != 0)
			{
				*entry |= PAGING_FLAG_USER;
			}
		}
		table = physical_to_virtual(*entry & PAGE_ADDRESS);
	}

	return &table[indices[3]];
}

bool paging_map_page(
		uint64_t physical_root,
		uintptr_t virtual_address,
		uint64_t physical_address,
		uint64_t flags
)
{
	if (!paging_initialized || physical_root == 0 ||
			((flags & PAGING_FLAG_NO_EXECUTE) != 0 && !nx_supported) ||
			!is_canonical(virtual_address) ||
			(virtual_address & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			(physical_address & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			physical_address > PAGE_ADDRESS || (flags & ~PAGE_ALLOWED_FLAGS) != 0 ||
			(physical_root & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			physical_root > PAGE_ADDRESS)
	{
		return false;
	}

	uint64_t *leaf = walk_to_leaf(physical_root, virtual_address, flags, true);
	if (leaf == 0 || (*leaf & PAGE_PRESENT) != 0)
	{
		return false;
	}

	*leaf = physical_address | PAGE_PRESENT | flags;
	if (physical_root == paging_current_address_space())
	{
		__asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
	}
	return true;
}

bool paging_unmap_page(uint64_t physical_root, uintptr_t virtual_address)
{
	if (!paging_initialized || physical_root == 0 || !is_canonical(virtual_address) ||
			(virtual_address & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			(physical_root & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			physical_root > PAGE_ADDRESS)
	{
		return false;
	}

	uint64_t *leaf = walk_to_leaf(physical_root, virtual_address, 0, false);
	if (leaf == 0 || (*leaf & PAGE_PRESENT) == 0)
	{
		return false;
	}

	*leaf = 0;
	if (physical_root == paging_current_address_space())
	{
		__asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
	}
	return true;
}

bool paging_get_mapping(
		uint64_t physical_root,
		uintptr_t virtual_address,
		uint64_t *physical_address,
		uint64_t *flags
)
{
	if (!paging_initialized || physical_root == 0 || !is_canonical(virtual_address) ||
			(physical_root & (PAGING_PAGE_SIZE - 1U)) != 0 ||
			physical_root > PAGE_ADDRESS)
	{
		return false;
	}

	uint64_t *leaf = walk_to_leaf(physical_root, virtual_address, 0, false);
	if (leaf == 0 || (*leaf & PAGE_PRESENT) == 0)
	{
		return false;
	}

	if (physical_address != 0)
	{
		*physical_address = (*leaf & PAGE_ADDRESS) |
				((uint64_t)virtual_address & (PAGING_PAGE_SIZE - 1U));
	}
	if (flags != 0)
	{
		*flags = *leaf & ~PAGE_ADDRESS;
	}
	return true;
}
