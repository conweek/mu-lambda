#ifndef MULAMBDA_READ_H
#define MULAMBDA_READ_H

#include <stddef.h>

#define MU_LINE_CANCELLED (-1) /* ctrl c */
#define MU_LINE_ERROR (-2)     /* bad args or console failure */

int mu_readline(char *buf, size_t cap, const char *prompt);

#endif
