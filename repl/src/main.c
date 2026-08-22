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

int main(void) {
    static char line[LINE_MAX];
    static char stmt[STMT_MAX];

    if (mu_console_init() < 0) {
        return -1;
    }

    mu_arena_init();

    env_t* env = create_env(NULL);
    if (!env) {
        return -1;
    }
    register_builtin(env, "print", builtin_print);
    register_builtin(env, "sleep", builtin_sleep);
    register_builtin(env, "gpioSet", builtin_gpio_set);
    register_builtin(env, "gpioRead", builtin_gpio_read);
    register_builtin(env, "i2cWrite", builtin_i2c_write);
    register_builtin(env, "i2cRead", builtin_i2c_read);

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
            value_release(result);
        }

        // Nothing was defined, so none of this needs to survive
        if (env->count == bindings_before) {
            memrina_restore_check(mu_session, cp);
        }

        if (batch) {
            mu_write(SEP_END, sizeof(SEP_END) - 1);
        }
    }
}
