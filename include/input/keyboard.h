#ifndef ETHOS_INPUT_KEYBOARD_H
#define ETHOS_INPUT_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_MOD_SHIFT 1U
#define KEYBOARD_MOD_CONTROL 2U

enum keyboard_key {
	KEYBOARD_KEY_CHARACTER = 0,
	KEYBOARD_KEY_UP,
	KEYBOARD_KEY_DOWN,
	KEYBOARD_KEY_LEFT,
	KEYBOARD_KEY_RIGHT,
	KEYBOARD_KEY_HOME,
	KEYBOARD_KEY_END,
	KEYBOARD_KEY_DELETE
};

struct keyboard_event {
	enum keyboard_key key;
	char character;
	uint8_t modifiers;
};

bool keyboard_init(void);
bool keyboard_poll_event(struct keyboard_event *event);
char keyboard_poll_char(void);

#endif
