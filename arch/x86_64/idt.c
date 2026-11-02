#include <arch/x86_64/idt.h>
#include <boot/boot.h>
#include <debug/serial.h>
#include <mm/paging.h>
#include <sched/scheduler.h>
#include <stdint.h>

// IDT entry for x86-64
struct __attribute__((packed)) idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t attributes;
  uint16_t offset_middle;
  uint32_t offset_high;
  uint32_t reserved;
};

// IDTR operand for LIDT
struct __attribute__((packed)) idt_pointer {
  uint16_t limit;
  uint64_t base;
};

#define IDT_ENTRIES         256
#define IDT_EXCEPTION_COUNT 32
#define IDT_IRQ_BASE        32
#define IDT_IRQ_COUNT       16

#define IDT_KERNEL_CODE_SELECTOR  0x08
#define IDT_INTERRUPT_GATE        0x8E
#define LOCAL_APIC_VIRTUAL_BASE   0xFFFFC00000001000ULL

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idtr;

// CPU exception stubs
#define DECLARE_ISR(n) extern void ethos_isr##n(void)

DECLARE_ISR(0);
DECLARE_ISR(1);
DECLARE_ISR(2);
DECLARE_ISR(3);
DECLARE_ISR(4);
DECLARE_ISR(5);
DECLARE_ISR(6);
DECLARE_ISR(7);
DECLARE_ISR(8);
DECLARE_ISR(9);
DECLARE_ISR(10);
DECLARE_ISR(11);
DECLARE_ISR(12);
DECLARE_ISR(13);
DECLARE_ISR(14);
DECLARE_ISR(15);
DECLARE_ISR(16);
DECLARE_ISR(17);
DECLARE_ISR(18);
DECLARE_ISR(19);
DECLARE_ISR(20);
DECLARE_ISR(21);
DECLARE_ISR(22);
DECLARE_ISR(23);
DECLARE_ISR(24);
DECLARE_ISR(25);
DECLARE_ISR(26);
DECLARE_ISR(27);
DECLARE_ISR(28);
DECLARE_ISR(29);
DECLARE_ISR(30);
DECLARE_ISR(31);

#undef DECLARE_ISR

// Hardware IRQ stubs
//
// PIC master : IRQ0-IRQ7   -> vectors 32-39
// PIC slave  : IRQ8-IRQ15  -> vectors 40-47

#define DECLARE_IRQ(n) extern void ethos_irq##n(void)

DECLARE_IRQ(0);
DECLARE_IRQ(1);
DECLARE_IRQ(2);
DECLARE_IRQ(3);
DECLARE_IRQ(4);
DECLARE_IRQ(5);
DECLARE_IRQ(6);
DECLARE_IRQ(7);
DECLARE_IRQ(8);
DECLARE_IRQ(9);
DECLARE_IRQ(10);
DECLARE_IRQ(11);
DECLARE_IRQ(12);
DECLARE_IRQ(13);
DECLARE_IRQ(14);
DECLARE_IRQ(15);

#undef DECLARE_IRQ

extern void idt_load(const struct idt_pointer *pointer);

static inline void outb(uint16_t port, uint8_t value)
{
  __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void set_idt_gate(uint8_t vector, uintptr_t handler)
{
  idt[vector] = (struct idt_entry) {
    .offset_low = (uint16_t)handler,
    .selector = IDT_KERNEL_CODE_SELECTOR,
    .ist = 0,
    .attributes = IDT_INTERRUPT_GATE,
    .offset_middle = (uint16_t)(handler >> 16),
    .offset_high = (uint32_t)(handler >> 32),
    .reserved = 0
  };
}

static void pic_remap(void)
{
  outb(0x20, 0x11);
  outb(0xA0, 0x11);
  outb(0x21, IDT_IRQ_BASE);
  outb(0xA1, IDT_IRQ_BASE + 8);
  outb(0x21, 0x04);
  outb(0xA1, 0x02);
  outb(0x21, 0x01);
  outb(0xA1, 0x01);
  outb(0x21, 0xFE);
  outb(0xA1, 0xFF);
}

static void route_pic_through_local_apic(void)
{
  uint32_t apic_base_low;
  uint32_t apic_base_high;
  __asm__ volatile (
    "rdmsr"
    : "=a"(apic_base_low), "=d"(apic_base_high)
    : "c"(0x1B)
  );
  (void)apic_base_high;
  if ((apic_base_low & (1U << 11)) == 0)
  {
    return;
  }

  uint64_t physical_base = apic_base_low & 0xFFFFF000U;
  if (!paging_map_page(
      paging_current_address_space(),
      LOCAL_APIC_VIRTUAL_BASE,
      physical_base,
      PAGING_FLAG_WRITE | PAGING_FLAG_WRITE_THROUGH | PAGING_FLAG_CACHE_DISABLE))
  {
    serial_write("ERROR >> Unable to map local APIC MMIO\n");
    return;
  }

  volatile uint32_t *lvt0 = (volatile uint32_t *)(uintptr_t)(
      LOCAL_APIC_VIRTUAL_BASE + 0x350U);
  *lvt0 = 0x700U;
  (void)*lvt0;
}

static void pit_init(void)
{
  uint16_t divisor = 1193182U / 100U;
  outb(0x43, 0x36);
  outb(0x40, (uint8_t)divisor);
  outb(0x40, (uint8_t)(divisor >> 8));
}

bool idt_init(void)
{
  if (idt[0].attributes != 0)
  {
    return false;
  }

  static void (*const exception_handlers[IDT_EXCEPTION_COUNT])(void) = {
    ethos_isr0, ethos_isr1, ethos_isr2, ethos_isr3,
    ethos_isr4, ethos_isr5, ethos_isr6, ethos_isr7,
    ethos_isr8, ethos_isr9, ethos_isr10, ethos_isr11,
    ethos_isr12, ethos_isr13, ethos_isr14, ethos_isr15,
    ethos_isr16, ethos_isr17, ethos_isr18, ethos_isr19,
    ethos_isr20, ethos_isr21, ethos_isr22, ethos_isr23,
    ethos_isr24, ethos_isr25, ethos_isr26, ethos_isr27,
    ethos_isr28, ethos_isr29, ethos_isr30, ethos_isr31
  };
  static void (*const irq_handlers[IDT_IRQ_COUNT])(void) = {
    ethos_irq0, ethos_irq1, ethos_irq2, ethos_irq3,
    ethos_irq4, ethos_irq5, ethos_irq6, ethos_irq7,
    ethos_irq8, ethos_irq9, ethos_irq10, ethos_irq11,
    ethos_irq12, ethos_irq13, ethos_irq14, ethos_irq15
  };

  for (uint32_t vector = 0; vector < IDT_EXCEPTION_COUNT; vector++)
  {
    set_idt_gate((uint8_t)vector, (uintptr_t)exception_handlers[vector]);
  }
  for (uint32_t irq = 0; irq < IDT_IRQ_COUNT; irq++)
  {
    set_idt_gate((uint8_t)(IDT_IRQ_BASE + irq), (uintptr_t)irq_handlers[irq]);
  }

  idtr.limit = sizeof(idt) - 1U;
  idtr.base = (uint64_t)(uintptr_t)idt;
  idt_load(&idtr);
  pic_remap();
  route_pic_through_local_apic();
  pit_init();
  serial_write("SUCCESS >> IDT, PIC, and PIT initialized\n");
  return true;
}

static void serial_write_hex(uint64_t value)
{
  static const char digits[] = "0123456789ABCDEF";
  char text[16];
  for (size_t index = 0; index < sizeof(text); index++)
  {
    text[sizeof(text) - index - 1U] = digits[value & 0xFU];
    value >>= 4;
  }
  for (size_t index = 0; index < sizeof(text); index++)
  {
    serial_write_char(text[index]);
  }
}

uintptr_t idt_exception_dispatch(uintptr_t stack)
{
  const uint64_t *frame = (const uint64_t *)stack;
  uint64_t vector = frame[15];
  bool from_user = (frame[18] & 3U) == 3U;

  if (from_user && scheduler_current_is_user())
  {
    serial_write("ERROR >> User process exception vector=0x");
    serial_write_hex(vector);
    serial_write(" error=0x");
    serial_write_hex(frame[16]);
    if (vector == 14)
    {
      uint64_t fault_address;
      __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
      serial_write(" address=0x");
      serial_write_hex(fault_address);
    }
    serial_write("\n");
    scheduler_fault_current(vector);
    return scheduler_exit_current();
  }

  serial_write("FATAL >> CPU exception vector=0x");
  serial_write_hex(vector);
  serial_write(" error=0x");
  serial_write_hex(frame[16]);
  if (vector == 14)
  {
    uint64_t fault_address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
    serial_write(" address=0x");
    serial_write_hex(fault_address);
  }
  serial_write("\n");
  __asm__ volatile ("cli" ::: "memory");
  for (;;)
  {
    __asm__ volatile ("hlt");
  }
}

uintptr_t idt_irq_dispatch(uintptr_t stack)
{
  const uint64_t *frame = (const uint64_t *)stack;
  uint64_t vector = frame[15];

  if (vector >= IDT_IRQ_BASE + 8)
  {
    outb(0xA0, 0x20);
  }
  outb(0x20, 0x20);

  if (vector == IDT_IRQ_BASE)
  {
    return scheduler_on_timer(stack);
  }
  return stack;
}
