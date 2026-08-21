#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mu_console.h"
#include "mu_read.h"

#define KEY_DEL 127
#define KEY_BS 8

// Escape sequences
enum { ESEQ_NONE, ESEQ_ESC, ESEQ_ESC_BRACKET, ESEQ_ESC_O };

static char last_nl;

/* Return true for \r and \n, prevent double newline on \r\n */
static bool is_newline(int c) {
  if ((c == '\r' || c == '\n') && (last_nl == 0 || last_nl == c)) {
    last_nl = (char)c;
    return true;
  }

  last_nl = 0;

  return false;
}

/* Move cursor left by n */
static void cursor_left(size_t n) {
  char cmd[16];
  int k;

  if (n == 0) {
    return;
  }

  // Backspaces are fewer bytes than the escape up to 4 chars
  if (n <= 4) {
    mu_write("\b\b\b\b", n);
    return;
  }

  k = snprintf(cmd, sizeof(cmd), "\x1b[%uD", (unsigned int)n);
  if (k > 0) {
    mu_write(cmd, (size_t)k);
  }
}

/* Reprint from cursor to end of line, erase leftovers, put cursor back */
static void redraw_tail(const char *buf, size_t len, size_t cursor) {
  mu_write(&buf[cursor], len - cursor);
  mu_write("\x1b[K", 3);
  cursor_left(len - cursor);
}

int mu_readline(char *buf, size_t cap, const char *prompt) {
  size_t len = 0;
  size_t cursor = 0;
  int esc = ESEQ_NONE;
  char param = 0;

  if (buf == NULL || cap == 0) {
    return MU_LINE_ERROR;
  }

  buf[0] = '\0';
  mu_write(prompt, strlen(prompt));

  for (;;) {
    int c = mu_getchar();

    // Input was lost, the line cannot be trusted
    if (mu_dropped()) {
      mu_write("\r\ninput dropped\r\n", 17);
      buf[0] = '\0';
      return MU_LINE_CANCELLED;
    }

    if (c < 0) { // ctrl c
      mu_write("^C\r\n", 4);
      buf[0] = '\0';
      return MU_LINE_CANCELLED;
    }

    // Escape sequences
    if (esc == ESEQ_ESC) {
      if (c == '[') {
        esc = ESEQ_ESC_BRACKET;
      } else if (c == 'O') {
        esc = ESEQ_ESC_O;
      } else {
        esc = ESEQ_NONE;
      }
      continue;
    }

    if (esc == ESEQ_ESC_BRACKET || esc == ESEQ_ESC_O) {
      if (esc == ESEQ_ESC_BRACKET && c >= 0x30 && c <= 0x3f) {
        // Params and ;, keep last digit
        if (c >= '0' && c <= '9') {
          param = (char)c;
        }
        continue;
      }
      if (esc == ESEQ_ESC_BRACKET && c >= 0x20 && c <= 0x2f) {
        continue; // intermediate bytes
      }

      esc = ESEQ_NONE;

      switch (c) {
      case 'C': // right
        if (cursor < len) {
          // Echoing the char steps the cursor right
          mu_putchar(buf[cursor]);
          cursor++;
        }
        break;
      case 'D': // left
        if (cursor > 0) {
          cursor--;
          cursor_left(1);
        }
        break;
      case '~':
        if (param == '3') { // delete at cursor
          if (cursor < len) {
            memmove(&buf[cursor], &buf[cursor + 1], len - cursor - 1);
            len--;
            redraw_tail(buf, len, cursor);
          }
        }
        break;
      default:
        break;
      }

      param = 0;
      continue;
    }

    if (c == 0x1b) {
      esc = ESEQ_ESC;
      continue;
    }

    // Editing
    if (is_newline(c)) {
      mu_write("\r\n", 2);
      buf[len] = '\0';
      return (int)len;
    }

    if (c == KEY_DEL || c == KEY_BS) { // delete before cursor
      if (cursor > 0) {
        memmove(&buf[cursor - 1], &buf[cursor], len - cursor);
        cursor--;
        len--;
        cursor_left(1);
        redraw_tail(buf, len, cursor);
      }
      continue;
    }

    // Drop remaining
    if (c < 0x20 || c > 0x7e) {
      continue;
    }

    if (len + 1 >= cap) { // leave room for null terminator
      mu_putchar('\a');
      continue;
    }

    // Insert at cursor and shift the tail
    memmove(&buf[cursor + 1], &buf[cursor], len - cursor);
    buf[cursor] = (char)c;
    len++;
    cursor++;
    mu_putchar((char)c);

    // No reprint needed when appending
    if (cursor < len) {
      redraw_tail(buf, len, cursor);
    }
  }
}
