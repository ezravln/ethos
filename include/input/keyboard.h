#ifndef ETHOS_INPUT_KEYBOARD_H
#define ETHOS_INPUT_KEYBOARD_H

#include <stdbool.h>

bool keyboard_init(void);
char keyboard_poll_char(void);

#endif
