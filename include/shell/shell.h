#ifndef ETHOS_SHELL_SHELL_H
#define ETHOS_SHELL_SHELL_H

#include <stdbool.h>

bool shell_init(void);
__attribute__((noreturn)) void shell_run(void);

#endif
