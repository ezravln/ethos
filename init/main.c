#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/memory.h>
#include <debug/serial.h>
#include <boot/limine/limine.h>
#include <arch/x86_64/gdt.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
  for (;;) {
    asm ("hlt");
  }
}

void kmain(void) {
  __asm__ volatile ("cli" ::: "memory");

  if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
    hcf();
  }

  serial_init();
  serial_write("INFO >> Initializing the required resources\n");

  gdt_init();

  serial_write("INFO >> Kernel initialized successful\n");

  hcf();
}
