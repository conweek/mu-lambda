#include "mu_console.h"
#include "mu_read.h"

#define LINE_MAX 128
#define BANNER "μλ REPL\r\n"

int main(void) {
  static char line[LINE_MAX];

  if (mu_console_init() < 0) {
    return -1;
  }

  mu_write(BANNER, sizeof(BANNER) - 1);

  for (;;) {
    int n = mu_readline(line, sizeof(line), ">>> ");

    // Ctrl c or bad args
    if (n < 0) {
      continue;
    }

    // if nothing typed
    if (n == 0) {
      continue;
    }

    // pass 'line' to evaluator when implemented
    mu_write(line, n);
    mu_write("\r\n", 2);
  }
}
