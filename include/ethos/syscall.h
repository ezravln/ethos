#ifndef ETHOS_USER_SYSCALL_H
#define ETHOS_USER_SYSCALL_H

#include <stdint.h>

#define ETHOS_SYSCALL_EXIT 0U
#define ETHOS_SYSCALL_WRITE_CHAR 1U
#define ETHOS_SYSCALL_SET_PRIORITY 2U
#define ETHOS_SYSCALL_GETPID 3U
#define ETHOS_SYSCALL_INPUT 4U

#define ETHOS_ERROR_PERMISSION 1
#define ETHOS_ERROR_NO_PROCESS 3
#define ETHOS_ERROR_TRY_AGAIN 11
#define ETHOS_ERROR_INVALID_ARGUMENT 22
#define ETHOS_ERROR_NOT_IMPLEMENTED 38

int64_t ethos_write_char(char character);
int64_t ethos_print(const char *text);
int64_t ethos_input(void);
int64_t ethos_getpid(void);
int64_t ethos_set_priority(uint8_t priority);
__attribute__((noreturn)) void ethos_exit(void);

#endif