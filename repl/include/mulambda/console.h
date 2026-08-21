#ifndef MULAMBDA_CONSOLE_H
#define MULAMBDA_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>

int mu_console_init(void);

int mu_getchar(void);

void mu_putchar(char c);
void mu_write(const char *s, size_t len);

bool mu_interrupted(void);

bool mu_dropped(void);

#endif
