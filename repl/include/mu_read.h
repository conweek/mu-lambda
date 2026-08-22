#ifndef MU_READ_H
#define MU_READ_H

#include <stddef.h>

#define MU_LINE_CANCELLED (-1) /* ctrl c */
#define MU_LINE_ERROR (-2)     /* bad args or console failure */
#define MU_LINE_EOF (-3)       /* ctrl d, ends batch mode */
#define MU_LINE_BATCH (-4)     /* ctrl e, starts batch mode */

int mu_readline(char *buf, size_t cap, const char *prompt);

#endif
