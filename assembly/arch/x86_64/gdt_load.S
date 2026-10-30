.section .text
.global gdt_load
.type gdt_load, @function

gdt_load:
lgdt (%rdi)
pushq $0x08
leaq .Lreload_cs(%rip), %rax
pushq %rax
lretq

.Lreload_cs:
movw $0x10, %ax
movw %ax, %ds
movw %ax, %es
movw %ax, %ss
movw %ax, %fs
movw %ax, %gs
ret

.size gdt_load, . - gdt_load

.global tss_load
.type tss_load, @function

tss_load:
  movw $0x28, %ax
  ltr %ax
  ret

.size tss_load, . - tss_load
.section .note.GNU-stack, "", @progbits
