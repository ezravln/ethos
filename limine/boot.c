#include <boot/boot.h>
#include <boot/limine/limine.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
  .id = LIMINE_MEMMAP_REQUEST_ID,
  .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
  .id = LIMINE_HHDM_REQUEST_ID,
  .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
  .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
  .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
  .id = LIMINE_MODULE_REQUEST_ID,
  .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

bool boot_init(void)
{
  return LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) &&
      memory_map_request.response != 0 &&
      memory_map_request.response->entry_count != 0 &&
      hhdm_request.response != 0;
}

struct limine_memmap_response *boot_memory_map(void)
{
  return memory_map_request.response;
}

struct limine_framebuffer *boot_framebuffer(void)
{
  if (framebuffer_request.response == 0 ||
      framebuffer_request.response->framebuffer_count == 0 ||
      framebuffer_request.response->framebuffers == 0)
  {
    return 0;
  }
  return framebuffer_request.response->framebuffers[0];
}

struct limine_module_response *boot_modules(void)
{
  return module_request.response;
}

uint64_t boot_hhdm_offset(void)
{
  return hhdm_request.response->offset;
}
