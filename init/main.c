#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/memory.h>
#include <debug/serial.h>
#include <boot/boot.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/syscall.h>
#include <mm/paging.h>
#include <proc/process.h>
#include <sched/scheduler.h>
#include <console/vga.h>
#include <fs/ramfs.h>
#include <shell/shell.h>

static void hcf(void) {
  for (;;) {
    asm ("hlt");
  }
}

void kmain(void) {
  __asm__ volatile ("cli" ::: "memory");

  serial_init();
  serial_write("INFO >> Initializing Ethos kernel\n");

  if (!boot_init())
  {
    serial_write("FATAL >> Limine boot information is unavailable\n");
    hcf();
  }

  if (!gdt_init() || !paging_init() || !idt_init() || !syscall_init() ||
      !scheduler_init() || !process_init() || !ramfs_init() || !shell_init())
  {
    serial_write("FATAL >> Kernel subsystem initialization failed\n");
    hcf();
  }

  if (!vga_init())
  {
    serial_write("ERROR >> VGA text console initialization failed\n");
  }
  else
  {
    vga_write("Ethos OS - x86_64\n");
    vga_write("Kernel subsystems initialized.\n");
  }

  static const char ramfs_test_data[] = "Ethos RAMFS is online.\n";
  char ramfs_verify_data[sizeof(ramfs_test_data)] = {0};
  size_t ramfs_bytes_read = 0;
    struct ramfs_file_info ramfs_info;
  if (!ramfs_create("/hello.txt") ||
      ramfs_create("/hello.txt") ||
      !ramfs_write("/hello.txt", 0, ramfs_test_data, sizeof(ramfs_test_data)) ||
      !ramfs_read("/hello.txt", 0, ramfs_verify_data,
          sizeof(ramfs_verify_data), &ramfs_bytes_read) ||
      ramfs_bytes_read != sizeof(ramfs_test_data) ||
      memcmp(ramfs_test_data, ramfs_verify_data, sizeof(ramfs_test_data)) != 0 ||
      !ramfs_stat("/hello.txt", &ramfs_info) ||
      ramfs_info.size != sizeof(ramfs_test_data) ||
      !ramfs_get_at(0, &ramfs_info) ||
      memcmp(ramfs_info.name, "hello.txt", sizeof("hello.txt")) != 0 ||
      ramfs_count() != 1 ||
      !ramfs_create("/remove.tmp") ||
      !ramfs_remove("/remove.tmp") ||
      ramfs_remove("/missing") ||
      ramfs_count() != 1)
  {
    serial_write("FATAL >> RAMFS read/write self-test failed\n");
    hcf();
  }
  serial_write("SUCCESS >> RAMFS create/write/read verified\n");
  if (vga_is_initialized())
  {
    vga_write("RAMFS ready: /hello.txt\n");
  }
  serial_write("SUCCESS >> Kernel subsystems initialized\n");
  if (!scheduler_start())
  {
    serial_write("FATAL >> Scheduler failed to start\n");
    hcf();
  }
  shell_run();
}
