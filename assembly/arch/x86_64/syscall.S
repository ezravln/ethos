.section .text
.extern syscall_dispatch
.extern ethos_syscall_kernel_stack

.global syscall_entry
.type syscall_entry, @function
syscall_entry:
  movq %rsp, syscall_user_stack(%rip)
  movq ethos_syscall_kernel_stack(%rip), %rsp
  pushq %rax
  pushq %rbx
  pushq %rcx
  pushq %rdx
  pushq %rbp
  pushq %rsi
  pushq %rdi
  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  subq $8, %rsp
  movq 56(%rsp), %rax
  movq %rax, (%rsp)
  movq 120(%rsp), %rdi
  movq 72(%rsp), %rsi
  movq 80(%rsp), %rdx
  movq 96(%rsp), %rcx
  movq 48(%rsp), %r8
  movq 64(%rsp), %r9
  call syscall_dispatch
  movq %rax, 120(%rsp)
  addq $8, %rsp
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %r11
  popq %r10
  popq %r9
  popq %r8
  popq %rdi
  popq %rsi
  popq %rbp
  popq %rdx
  popq %rcx
  popq %rbx
  popq %rax
  movq syscall_user_stack(%rip), %rsp
  sysretq
.size syscall_entry, . - syscall_entry

.global enter_user_mode
.type enter_user_mode, @function
enter_user_mode:
  cli
  movw $0x1B, %ax
  movw %ax, %ds
  movw %ax, %es
  pushq $0x1B
  pushq %rsi
  pushfq
  popq %rax
  orq $0x200, %rax
  pushq %rax
  pushq $0x23
  pushq %rdi
  iretq
.size enter_user_mode, . - enter_user_mode

.bss
.align 8
syscall_user_stack:
  .quad 0

.section .note.GNU-stack,"",@progbits