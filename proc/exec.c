#include <boot/boot.h>
#include <boot/limine/limine.h>
#include <debug/serial.h>
#include <mm/paging.h>
#include <proc/apx.h>
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
#define ELF_ARGUMENT_MAX 64U

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

static bool write_user_memory(uint64_t address_space, uintptr_t destination,
    const void *source, size_t size)
{
  const uint8_t *source_bytes = source;
  while (size != 0)
  {
    uintptr_t page_address = destination &
        ~(uintptr_t)(PAGING_PAGE_SIZE - 1U);
    uint64_t physical_address;
    if (!paging_get_mapping(address_space, page_address, &physical_address, 0))
    {
      return false;
    }

    size_t page_offset = destination - page_address;
    size_t chunk_size = PAGING_PAGE_SIZE - page_offset;
    if (chunk_size > size)
    {
      chunk_size = size;
    }
    uint8_t *target = (uint8_t *)(uintptr_t)(boot_hhdm_offset() +
        physical_address + page_offset);
    for (size_t index = 0; index < chunk_size; index++)
    {
      target[index] = source_bytes[index];
    }
    destination += chunk_size;
    source_bytes += chunk_size;
    size -= chunk_size;
  }
  return true;
}

static bool build_user_stack(uint64_t address_space, size_t argument_count,
    char *const arguments[], uintptr_t *stack_pointer)
{
  uintptr_t stack_start = USER_STACK_TOP - USER_STACK_PAGES * PAGING_PAGE_SIZE;
  uintptr_t cursor = USER_STACK_TOP;
  uintptr_t argument_addresses[ELF_ARGUMENT_MAX];
  uint64_t stack_words[ELF_ARGUMENT_MAX + 3U] = {0};

  if (argument_count == 0 || argument_count > ELF_ARGUMENT_MAX ||
      arguments == 0 || stack_pointer == 0)
  {
    return false;
  }

  for (size_t index = argument_count; index != 0; index--)
  {
    const char *argument = arguments[index - 1U];
    if (argument == 0)
    {
      return false;
    }
    size_t length = 0;
    while (argument[length] != '\0')
    {
      length++;
    }
    length++;
    if (length > cursor - stack_start)
    {
      return false;
    }
    cursor -= length;
    if (!write_user_memory(address_space, cursor, argument, length))
    {
      return false;
    }
    argument_addresses[index - 1U] = cursor;
  }

  stack_words[0] = argument_count;
  for (size_t index = 0; index < argument_count; index++)
  {
    stack_words[index + 1U] = argument_addresses[index];
  }
  size_t word_count = argument_count + 3U;
  size_t stack_size = word_count * sizeof(stack_words[0]);
  if (stack_size > cursor - stack_start)
  {
    return false;
  }
  uintptr_t initial_stack = (cursor - stack_size) & ~(uintptr_t)0xFU;
  if (initial_stack < stack_start ||
      !write_user_memory(address_space, initial_stack, stack_words,
          stack_size))
  {
    return false;
  }
  *stack_pointer = initial_stack;
  return true;
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
      paging_free_page(backing_page);
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
  size_t argument_count, char *const arguments[])
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
      goto load_failed;
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
    goto load_failed;
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
      paging_free_page(backing_page);
      goto load_failed;
    }
  }

  uint64_t process_id;
  uintptr_t initial_stack;
  if (!build_user_stack(address_space, argument_count, arguments,
      &initial_stack) || !process_create(arguments[0],
      (uintptr_t)header->entry, initial_stack, address_space, &process_id))
  {
    goto load_failed;
  }
  return process_wait(process_id);

load_failed:
  paging_destroy_address_space(address_space);
  return false;
}

static bool load_apx_module(const struct limine_file *module,
  size_t argument_count, char *const arguments[])
{
  if (module == 0 || module->address == 0 ||
      module->size < sizeof(struct apx_header))
  {
    return false;
  }

  const struct apx_header *header = module->address;
  static const uint8_t apx_magic[ETHOS_APX_MAGIC_SIZE] = ETHOS_APX_MAGIC;
  for (size_t index = 0; index < sizeof(apx_magic); index++)
  {
    if (header->magic[index] != apx_magic[index])
    {
      return false;
    }
  }
  if (header->version != ETHOS_APX_VERSION ||
      header->header_size < sizeof(*header) ||
      header->header_size >= module->size ||
      header->architecture != ETHOS_APX_ARCH_X86_64 || header->flags != 0)
  {
    return false;
  }

  struct limine_file payload = *module;
  payload.address = (void *)((uintptr_t)module->address + header->header_size);
  payload.size -= header->header_size;
  return load_elf_module(&payload, argument_count, arguments);
}

bool process_exec_command(size_t argument_count, char *const arguments[])
{
  if (argument_count == 0 || argument_count > ELF_ARGUMENT_MAX ||
      arguments == 0 || arguments[0] == 0 || arguments[0][0] == '\0')
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
    if (module != 0 && strings_equal(module->string, arguments[0]))
    {
      return load_apx_module(module, argument_count, arguments);
    }
  }
  return false;
}
