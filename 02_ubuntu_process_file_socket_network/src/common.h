#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

void read_line(const char *prompt, char *buffer, size_t size);
int read_int(const char *prompt);
void pause_enter(void);

#endif
