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
#include <net/ethernet.h>
#include <net/wifi.h>

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

  if (ethernet_init())
  {
    serial_write("SUCCESS >> E1000 Ethernet adapter initialized\n");
  }
  else
  {
    serial_write("INFO >> No supported E1000 Ethernet adapter found\n");
  }

  struct wifi_device_info wifi_info;
  if (iwl_ax201_probe(&wifi_info) && wifi_info.detected)
  {
    serial_write(wifi_info.firmware_available
        ? "INFO >> Intel AX201 detected; firmware module available, radio not initialized\n"
        : "INFO >> Intel AX201 detected; firmware module missing, radio not initialized\n");
  }
  else if (wifi_info.firmware_available)
  {
    serial_write("INFO >> AX201 firmware module loaded; no AX201 device detected\n");
  }

  static const char ramfs_test_data[] = "Ethos RAMFS is online.\n";
  char ramfs_verify_data[sizeof(ramfs_test_data)] = {0};
  size_t ramfs_bytes_read = 0;
  struct ramfs_file_info ramfs_info;
  struct ramfs_directory_info directory_info;
  if (!ramfs_is_directory("/bin") ||
      !ramfs_is_directory("/sbin") ||
      !ramfs_is_directory("/lib/kernel") ||
      !ramfs_is_directory("/home/ethos/Documents") ||
      !ramfs_is_directory("/home/ethos/Media/Pictures") ||
      ramfs_directory_count() != 29U ||
      !ramfs_directory_get_at(0, &directory_info) ||
      memcmp(directory_info.path, "/", sizeof("/")) != 0 ||
      directory_info.read_only ||
      ramfs_create("/boot/modified") ||
      !ramfs_mkdir("/home/ethos/Documents/Notes") ||
      !ramfs_is_directory("/home/ethos/Documents/Notes") ||
      !ramfs_create("/home/ethos/Documents/layout-check.tmp") ||
      !ramfs_remove("/home/ethos/Documents/layout-check.tmp") ||
      !ramfs_create("/hello.txt") ||
      ramfs_create("/hello.txt") ||
      !ramfs_write("/hello.txt", 0, ramfs_test_data, sizeof(ramfs_test_data)) ||
      !ramfs_read("/hello.txt", 0, ramfs_verify_data,
          sizeof(ramfs_verify_data), &ramfs_bytes_read) ||
      ramfs_bytes_read != sizeof(ramfs_test_data) ||
      memcmp(ramfs_test_data, ramfs_verify_data, sizeof(ramfs_test_data)) != 0 ||
      !ramfs_stat("/hello.txt", &ramfs_info) ||
      ramfs_info.size != sizeof(ramfs_test_data) ||
      !ramfs_get_at(0, &ramfs_info) ||
      memcmp(ramfs_info.path, "/hello.txt", sizeof("/hello.txt")) != 0 ||
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
    vga_write("RAMFS ready: /home/ethos\n");
  }
  serial_write("SUCCESS >> Kernel subsystems initialized\n");
  if (!scheduler_start())
  {
    serial_write("FATAL >> Scheduler failed to start\n");
    hcf();
  }
  shell_run();
}
