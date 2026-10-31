#ifndef ETHOS_DEBUG_SERIAL_H
#define ETHOS_DEBUG_SERIAL_H

void serial_init(void);
void serial_write_char(char c);
void serial_write(const char* str);

char serial_read_char(void);

#endif
