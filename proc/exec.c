#include <boot/boot.h>
#include <boot/limine/limine.h>
#include <debug/serial.h>
#include <mm/paging.h>
#include <proc/process.h>
#include <stdint.h>

#define ELF_EXECUTABLE 2U
#define ELF_X86_64 62U
#define ELF_LOAD 1U
#define ELF_FLAG_EXECUTE 1U
#define ELF_FLAG_WRITE 2U
#define USER_ADDRESS_LIMIT 0x0000800000000000ULL
#define USER_STACK_TOP 0x00007FFFFFFFE000ULL
#define USER_STACK_PAGES 8U

struct __attribute__((packed)) elf64_header {
  uint8_t ident[16];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t program_header_offset;
  uint64_t section_header_offset;
  uint32_t flags;
  uint16_t header_size;
  uint16_t program_header_size;
  uint16_t program_header_count;
  uint16_t section_header_size;
  uint16_t section_header_count;
  uint16_t section_name_index;
};

struct __attribute__((packed)) elf64_program_header {
  uint32_t type;
  uint32_t flags;
  uint64_t offset;
  uint64_t virtual_address;
  uint64_t physical_address;
  uint64_t file_size;
  uint64_t memory_size;
  uint64_t alignment;
};

static bool strings_equal(const char *left, const char *right)
{
  if (left == 0 || right == 0)
  {
    return false;
  }
  size_t index = 0;
  while (left[index] != '\0' && right[index] != '\0')
  {
    if (left[index] != right[index])
    {
      return false;
    }
    index++;
  }
  return left[index] == right[index];
}

static bool range_inside(uint64_t offset, uint64_t size, uint64_t limit)
{
  return offset <= limit && size <= limit - offset;
}

static bool map_elf_segment(
    uint64_t address_space,
    const uint8_t *image,
    uint64_t image_size,
    const struct elf64_program_header *segment
)
{
  if (segment->memory_size == 0 || segment->file_size > segment->memory_size ||
      !range_inside(segment->offset, segment->file_size, image_size) ||
      segment->virtual_address >= USER_ADDRESS_LIMIT ||
      segment->memory_size > USER_ADDRESS_LIMIT - segment->virtual_address ||
      segment->virtual_address + segment->memory_size > UINT64_MAX -
          (PAGING_PAGE_SIZE - 1U))
  {
    return false;
  }

  uint64_t page_start = segment->virtual_address &
      ~(uint64_t)(PAGING_PAGE_SIZE - 1U);
  uint64_t segment_end = segment->virtual_address + segment->memory_size;
  uint64_t page_end = (segment_end + PAGING_PAGE_SIZE - 1U) &
      ~(uint64_t)(PAGING_PAGE_SIZE - 1U);
  uint64_t flags = PAGING_FLAG_USER;
  if ((segment->flags & ELF_FLAG_WRITE) != 0)
  {
    flags |= PAGING_FLAG_WRITE;
  }
  if ((segment->flags & ELF_FLAG_EXECUTE) == 0)
  {
    flags |= PAGING_FLAG_NO_EXECUTE;
  }

  for (uint64_t page = page_start; page < page_end; page += PAGING_PAGE_SIZE)
  {
    void *backing_page = paging_alloc_page();
    if (backing_page == 0)
    {
      return false;
    }
    uint64_t physical_address = (uint64_t)(uintptr_t)backing_page -
        boot_hhdm_offset();
    if (!paging_map_page(address_space, (uintptr_t)page,
        physical_address, flags))
    {
      return false;
    }
  }

  uint64_t copied = 0;
  while (copied < segment->file_size)
  {
    uint64_t virtual_address = segment->virtual_address + copied;
    uint64_t page_address = virtual_address &
        ~(uint64_t)(PAGING_PAGE_SIZE - 1U);
    uint64_t page_offset = virtual_address - page_address;
    uint64_t chunk_size = PAGING_PAGE_SIZE - page_offset;
    if (chunk_size > segment->file_size - copied)
    {
      chunk_size = segment->file_size - copied;
    }

    uint64_t physical_address;
    if (!paging_get_mapping(address_space, (uintptr_t)page_address,
        &physical_address, 0))
    {
      return false;
    }
    uint8_t *destination = (uint8_t *)(uintptr_t)(
        boot_hhdm_offset() + physical_address + page_offset);
    const uint8_t *source = image + segment->offset + copied;
    for (uint64_t index = 0; index < chunk_size; index++)
    {
      destination[index] = source[index];
    }
    copied += chunk_size;
  }
  return true;
}

static bool load_elf_module(const struct limine_file *module,
    const char *command)
{
  if (module == 0 || module->address == 0 ||
      module->size < sizeof(struct elf64_header))
  {
    return false;
  }

  const uint8_t *image = module->address;
  const struct elf64_header *header = (const struct elf64_header *)image;
  static const uint8_t elf_magic[4] = {0x7F, 'E', 'L', 'F'};
  for (size_t index = 0; index < sizeof(elf_magic); index++)
  {
    if (header->ident[index] != elf_magic[index])
    {
      return false;
    }
  }
  if (header->ident[4] != 2 || header->ident[5] != 1 ||
      header->type != ELF_EXECUTABLE || header->machine != ELF_X86_64 ||
      header->version != 1 || header->header_size != sizeof(*header) ||
      header->program_header_size != sizeof(struct elf64_program_header) ||
      header->program_header_count == 0 || header->program_header_count > 32 ||
      !range_inside(header->program_header_offset,
          (uint64_t)header->program_header_count * header->program_header_size,
          module->size) || header->entry >= USER_ADDRESS_LIMIT)
  {
    return false;
  }

  uint64_t address_space = paging_create_address_space();
  if (address_space == 0)
  {
    return false;
  }

  const struct elf64_program_header *program_headers =
      (const struct elf64_program_header *)(image + header->program_header_offset);
  bool entry_executable = false;
  for (uint16_t index = 0; index < header->program_header_count; index++)
  {
    const struct elf64_program_header *segment = &program_headers[index];
    if (segment->type != ELF_LOAD || segment->memory_size == 0)
    {
      continue;
    }
    if (!map_elf_segment(address_space, image, module->size, segment))
    {
      return false;
    }
    if ((segment->flags & ELF_FLAG_EXECUTE) != 0 &&
        header->entry >= segment->virtual_address &&
        header->entry - segment->virtual_address < segment->memory_size)
    {
      entry_executable = true;
    }
  }
  if (!entry_executable)
  {
    return false;
  }

  uint64_t stack_start = USER_STACK_TOP - USER_STACK_PAGES * PAGING_PAGE_SIZE;
  for (uint64_t page = stack_start; page < USER_STACK_TOP; page += PAGING_PAGE_SIZE)
  {
    void *backing_page = paging_alloc_page();
    if (backing_page == 0)
    {
      return false;
    }
    uint64_t physical_address = (uint64_t)(uintptr_t)backing_page -
        boot_hhdm_offset();
    if (!paging_map_page(address_space, (uintptr_t)page,
        physical_address, PAGING_FLAG_USER | PAGING_FLAG_WRITE |
            PAGING_FLAG_NO_EXECUTE))
    {
      return false;
    }
  }

  uint64_t process_id;
  if (!process_create(command, (uintptr_t)header->entry,
      USER_STACK_TOP, address_space, &process_id))
  {
    return false;
  }
  return process_wait(process_id);
}

bool process_exec_command(const char *command)
{
  if (command == 0 || command[0] == '\0')
  {
    return false;
  }

  struct limine_module_response *modules = boot_modules();
  if (modules == 0 || modules->modules == 0)
  {
    return false;
  }
  for (uint64_t index = 0; index < modules->module_count; index++)
  {
    struct limine_file *module = modules->modules[index];
    if (module != 0 && strings_equal(module->string, command))
    {
      return load_elf_module(module, command);
    }
  }
  return false;
}
