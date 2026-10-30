#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/memory.h>
#include <debug/serial.h>
#include <boot/boot.h>
#include <arch/x86_64/gdt.h>

static void hcf(void) {
  for (;;) {
    asm ("hlt");
  }
}

void kmain(void) {
  __asm__ volatile ("cli" ::: "memory");

  if (!boot_init())
  {
    hcf();
  }

  serial_init();
  serial_write("INFO >> Initializing the required resources\n");

  gdt_init();

  serial_write("INFO >> Kernel initialized successful\n");

  hcf();
}
