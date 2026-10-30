#include <debug/serial.h>
#include <arch/x86_64/gdt.h>
#include <stdint.h>

#define GDT_ENTRY_COUNT 7U
#define TSS_DESCRIPTOR_INDEX 5U
#define BOOT_KERNEL_STACK_SIZE (16U  * 1024U)

struct __attribute__((packed)) gdt_pointer
{
  uint16_t limit;
  uint64_t base;
};

struct __attribute__((packed)) tss64
{
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iomap_base;
};

_Static_assert(sizeof(struct tss64) == 104, "TSS64 size mismatch");

static uint64_t gdt[GDT_ENTRY_COUNT];
static struct gdt_pointer gdtr;
static struct tss64 tss;
static uint8_t boot_kernel_stack[BOOT_KERNEL_STACK_SIZE]
  __attribute__((aligned(16)));

extern void gdt_load(const struct gdt_pointer *pointer);
extern void tss_load(void);

static uint64_t make_tss_descriptor(uintptr_t base, uint32_t limit)
{
  return 
    ((uint64_t)(limit & 0xFFFF)) |
    ((uint64_t)(base & 0xFFFFFF) << 16) |
    (0x89ULL << 40) |
    ((uint64_t)((limit >> 16) & 0xF) << 48) |
    ((uint64_t)((base >> 24) & 0xF) << 56);
}

bool gdt_init(void)
{
  gdt[0] = 0;
  gdt[1] = 0x00AF9A000000FFFFULL;
  gdt[2] = 0x00CF92000000FFFFULL;
  gdt[3] = 0x00CFF2000000FFFFULL;
  gdt[4] = 0x00AFFA000000FFFFULL;

  tss = (struct tss64) {
    .rsp0 = gdt_boot_stack_top(),
    .iomap_base = sizeof(tss)
  };

  uintptr_t tss_base = (uintptr_t)&tss;
  uint32_t tss_limit = sizeof(tss) - 1;
  gdt[TSS_DESCRIPTOR_INDEX] = make_tss_descriptor(tss_base, tss_limit);
  gdt[TSS_DESCRIPTOR_INDEX + 1] = (uint64_t)(tss_base >> 32);

  gdtr.limit = sizeof(gdt) - 1;
  gdtr.base = (uint64_t)(uintptr_t)gdt;

  gdt_load(&gdtr);
  tss_load();

  serial_write("SUCCESS >> GDT initialized with ring-3 segments and TSS\n");

  return true;
}

void gdt_set_kernel_stack(uintptr_t stack_top)
{
  tss.rsp0 = stack_top;
}

uintptr_t gdt_boot_stack_top(void)
{
  return (uintptr_t)&boot_kernel_stack[BOOT_KERNEL_STACK_SIZE];
}
