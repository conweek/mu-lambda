#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mu_arena.h"
#include "mu_console.h"
#include "mu_read.h"
#include "interpreter.h"
#include "builtins.h"

#define LINE_MAX  128
#define STMT_MAX  4096
#define BANNER    "μλ REPL\r\n"
#define BATCH_MSG "batch mode, ctrl d to run\r\n"
#define SEP_IN    "=============== input ===============\r\n"
#define SEP_OUT   "=============== output ==============\r\n"
#define SEP_END   "================ end ================\r\n"
/* Put the terminal back the way it was found: default colours, cursor on */
#define TERM_RESET "\x1b[0m\x1b[?25h"
#define RESET_MSG "session cleared\r\n"

/* Buffer uses \n between lines, terminal needs \r\n */
static void mu_print(const char* s, size_t len) {
    size_t start = 0;

    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\n') {
            mu_write(s + start, i - start);
            mu_write("\r\n", 2);
            start = i + 1;
        }
    }

    mu_write(s + start, len - start);
}

/* Ending a line with ':' will start writing a block, 'enter' on an empty line
 * ends the block */
static bool opens_block(const char* s, size_t len) {
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        len--;
    }

    return len > 0 && s[len - 1] == ':';
}

static env_t* fresh_env(void) {
    env_t* env = create_env(NULL);

    if (!env) {
        return NULL;
    }

    register_builtin(env, "print", builtin_print);
    register_builtin(env, "write", builtin_write);
    register_builtin(env, "halt", builtin_halt);
    register_builtin(env, "reset", builtin_reset);
    register_builtin(env, "sleep", builtin_sleep);
    register_builtin(env, "gpioSet", builtin_gpio_set);
    register_builtin(env, "gpioRead", builtin_gpio_read);
    register_builtin(env, "i2cRegWrite", builtin_i2c_reg_write);
    register_builtin(env, "i2cRegRead", builtin_i2c_reg_read);

    return env;
}

int main(void) {
    static char line[LINE_MAX];
    static char stmt[STMT_MAX];

    if (mu_console_init() < 0) {
        return -1;
    }

    mu_arena_init();

    env_t* env = fresh_env();
    if (!env) {
        return -1;
    }

    mu_write(BANNER, sizeof(BANNER) - 1);

    for (;;) {
        const char* prompt = ">>> ";
        size_t len = 0;
        bool block = false;
        bool batch = false;

        stmt[0] = '\0';

        for (;;) {
            int n = mu_readline(line, sizeof(line), prompt);

            // Ctrl e, keep collecting lines until ctrl d
            if (n == MU_LINE_BATCH) {
                // Ignore it if already in batch mode
                if (!batch) {
                    mu_write(BATCH_MSG, sizeof(BATCH_MSG) - 1);
                    mu_write(SEP_IN, sizeof(SEP_IN) - 1);
                    batch = true;
                    prompt = "";
                }
                continue;
            }

            // Ctrl d, end batch mode
            if (n == MU_LINE_EOF) {
                break;
            }

            // Ctrl c or bad args
            if (n < 0) {
                len = 0;
                break;
            }

            // blank line, close the open block
            if (block && !batch && n == 0) {
                break;
            }

            if (len + n + 2 > sizeof(stmt)) {
                mu_write("too long\r\n", 10);
                len = 0;
                break;
            }

            memcpy(stmt + len, line, n);
            len += n;
            stmt[len] = '\0';

            if (opens_block(line, n)) {
                block = true;
            }

            if (!block && !batch) {
                break;
            }

            stmt[len++] = '\n';
            stmt[len] = '\0';

            if (!batch) {
                prompt = "... ";
            }
        }

        // if nothing typed
        if (len == 0) {
            continue;
        }

        if (batch) {
            mu_write(SEP_OUT, sizeof(SEP_OUT) - 1);
        }

        // Scratch only holds the token list, it never outlives a submission
        memrina_clear(mu_scratch);

        // Rewind point for a submission that turns out to define nothing
        Memrina_Checkpoint cp = memrina_set_check(mu_session);
        int bindings_before = env->count;

        // Source has to outlive the call, tokens point into it
        char* src = memrina_strndup(mu_session, stmt, len);
        if (!src) {
            mu_write("out of memory\r\n", 16);
            continue;
        }

        value_t* result = run_interpreter(src, env);

        /* A program that set colours or hid the cursor may have been cut off
         * before it could undo either, so the prompt never inherits them. */
        mu_write(TERM_RESET, sizeof(TERM_RESET) - 1);

        if (result) {
            if (result->valueType == VAR_INT) {
                char buf[16];
                int n = snprintf(buf, sizeof(buf), "%d", result->value.integer);
                mu_write(buf, n);
                mu_write("\r\n", 2);
            } else if (result->valueType == VAR_STRING) {
                mu_write(result->value.string, strlen(result->value.string));
                mu_write("\r\n", 2);
            }
        }

        /* Keep a submission only if it ran to the end and defined something.
         * An interrupt or an error rolls the whole thing back, dropping its
         * half made definitions along with everything it allocated, so the
         * next one starts from exactly where this one did. */
        if (mu_reset_requested()) {
            // Safe now that evaluation has unwound and nothing points into it
            memrina_clear(mu_session);
            env = fresh_env();

            if (!env) {
                return -1;
            }

            mu_write(RESET_MSG, sizeof(RESET_MSG) - 1);
        } else if (!result || env->count == bindings_before) {
            env_truncate(env, bindings_before);
            memrina_restore_check(mu_session, cp);
        }

        if (batch) {
            mu_write(SEP_END, sizeof(SEP_END) - 1);
        }
    }
}
