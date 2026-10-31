#include <arch/x86_64/idt.h>
#include <debug/serial.h>
#include <scheduler/scheduler.h>
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
