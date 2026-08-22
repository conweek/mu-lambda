#ifndef __BUILTINS_H
#define __BUILTINS_H

#include "interpreter.h"

value_t* builtin_print(value_t* arg);
value_t* builtin_sleep(value_t* arg);
value_t* builtin_gpio_set(value_t* arg);
value_t* builtin_gpio_read(value_t* arg);
value_t* builtin_i2c_write(value_t* arg);
value_t* builtin_i2c_read(value_t* arg);
value_t* builtin_i2c_reg_write(value_t* arg);
value_t* builtin_i2c_reg_read(value_t* arg);
void register_builtin(env_t* env, const char* name, builtin_fn fn);

#endif
