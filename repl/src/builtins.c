#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include "interpreter.h"
#include "zephyr/dt-bindings/gpio/gpio.h"
#include "builtins.h"
#include "mu_arena.h"

#define BUILTIN_ERR(name)                                                                          \
    do {                                                                                           \
        printk("[!] Error: " name " syntax error\n");                                              \
        return make_error();                                                                       \
    } while (0)

value_t* builtin_print(value_t* arg) {
    if (!arg) {
        BUILTIN_ERR("print");
    }
    if (arg->valueType == VAR_INT) {
        printk("%d\n", arg->value.integer);
    } else if (arg->valueType == VAR_STRING) {
        printk("%s\n", arg->value.string);
    } else {
        BUILTIN_ERR("print");
    }
    return make_no_result();
}

value_t* builtin_sleep(value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("sleep");
    }

    k_msleep(arg->value.integer);
    return make_no_result();
}

typedef struct {
    const struct device* gpio;
    int pin;
} gpio_ctx_t;

static value_t* gpio_set_value(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("gpioSet");
    }

    gpio_ctx_t* g = (gpio_ctx_t*)ctx;

    // TODO: Don't enforce PULL_UP, let user decide
    gpio_pin_configure(g->gpio, g->pin, GPIO_OUTPUT | GPIO_PULL_UP);
    gpio_pin_set(g->gpio, g->pin, arg->value.integer);

    return make_no_result();
}

static value_t* gpio_set_pin(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("gpioSet");
    }

    gpio_ctx_t* g = (gpio_ctx_t*)ctx;

    gpio_ctx_t* next = (gpio_ctx_t*)memrina_alloc(mu_session, sizeof(gpio_ctx_t));
    if (!next) {
        return NULL;
    }

    next->gpio = g->gpio;
    next->pin = arg->value.integer;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = gpio_set_value;
    val->value.native.ctx = next;

    return val;
}

value_t* builtin_gpio_set(value_t* arg) {
    if (!arg || arg->valueType != VAR_STRING) {
        BUILTIN_ERR("gpioSet");
    }

    const struct device* gpio = device_get_binding(arg->value.string);
    if (!gpio || !device_is_ready(gpio)) {
        printk("[!] Error: gpioSet device '%s' not ready\n", arg->value.string);
        return make_error();
    }

    gpio_ctx_t* ctx = (gpio_ctx_t*)memrina_alloc(mu_session, sizeof(gpio_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->gpio = gpio;
    ctx->pin = -1;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = gpio_set_pin;
    val->value.native.ctx = ctx;

    return val;
}

static value_t* gpio_read_pin(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("gpioRead");
    }

    gpio_ctx_t* g = (gpio_ctx_t*)ctx;
    gpio_pin_configure(g->gpio, arg->value.integer, GPIO_INPUT);
    int val = gpio_pin_get(g->gpio, arg->value.integer);

    return make_int(val);
}

value_t* builtin_gpio_read(value_t* arg) {
    if (!arg || arg->valueType != VAR_STRING) {
        BUILTIN_ERR("gpioRead");
    }

    const struct device* gpio = device_get_binding(arg->value.string);
    if (!gpio || !device_is_ready(gpio)) {
        printk("[!] Error: gpioRead device '%s' not ready\n", arg->value.string);
        return make_error();
    }

    gpio_ctx_t* ctx = (gpio_ctx_t*)memrina_alloc(mu_session, sizeof(gpio_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->gpio = gpio;
    ctx->pin = -1;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = gpio_read_pin;
    val->value.native.ctx = ctx;

    return val;
}

typedef struct {
    const struct device* i2c;
    uint16_t addr;
    uint8_t reg;
} i2c_reg_ctx_t;

static value_t* i2c_reg_write_val(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("i2cRegWrite");
    }

    i2c_reg_ctx_t* c = (i2c_reg_ctx_t*)ctx;
    uint8_t buf[2] = { c->reg, (uint8_t)arg->value.integer };

    int ret = i2c_write(c->i2c, buf, 2, c->addr);
    if (ret < 0) {
        printk("[!] Error: i2cRegWrite failed (%d)\n", ret);
        return make_error();
    }

    return make_no_result();
}

static value_t* i2c_reg_write_reg(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("i2cRegWrite");
    }

    i2c_reg_ctx_t* prev = (i2c_reg_ctx_t*)ctx;

    i2c_reg_ctx_t* next = (i2c_reg_ctx_t*)memrina_alloc(mu_session, sizeof(i2c_reg_ctx_t));
    if (!next) {
        return NULL;
    }

    next->i2c = prev->i2c;
    next->addr = prev->addr;
    next->reg = (uint8_t)arg->value.integer;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = i2c_reg_write_val;
    val->value.native.ctx = next;

    return val;
}

static value_t* i2c_reg_write_addr(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("i2cRegWrite");
    }

    i2c_reg_ctx_t* prev = (i2c_reg_ctx_t*)ctx;

    i2c_reg_ctx_t* next = (i2c_reg_ctx_t*)memrina_alloc(mu_session, sizeof(i2c_reg_ctx_t));
    if (!next) {
        return NULL;
    }

    next->i2c = prev->i2c;
    next->addr = (uint16_t)arg->value.integer;
    next->reg = 0;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = i2c_reg_write_reg;
    val->value.native.ctx = next;

    return val;
}

value_t* builtin_i2c_reg_write(value_t* arg) {
    if (!arg || arg->valueType != VAR_STRING) {
        BUILTIN_ERR("i2cRegWrite");
    }

    const struct device* i2c = device_get_binding(arg->value.string);
    if (!i2c || !device_is_ready(i2c)) {
        printk("[!] Error: i2cRegWrite device '%s' not ready\n", arg->value.string);
        return make_error();
    }

    i2c_reg_ctx_t* ctx = (i2c_reg_ctx_t*)memrina_alloc(mu_session, sizeof(i2c_reg_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->i2c = i2c;
    ctx->addr = 0;
    ctx->reg = 0;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = i2c_reg_write_addr;
    val->value.native.ctx = ctx;

    return val;
}

static value_t* i2c_reg_read_reg(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("i2cRegRead");
    }

    i2c_reg_ctx_t* c = (i2c_reg_ctx_t*)ctx;
    uint8_t reg = (uint8_t)arg->value.integer;
    uint8_t byte = 0;

    int ret = i2c_reg_read_byte(c->i2c, c->addr, reg, &byte);
    if (ret < 0) {
        printk("[!] Error: i2cRegRead failed (%d)\n", ret);
        return make_error();
    }

    return make_int((int)byte);
}

static value_t* i2c_reg_read_addr(void* ctx, value_t* arg) {
    if (!arg || arg->valueType != VAR_INT) {
        BUILTIN_ERR("i2cRegRead");
    }

    i2c_reg_ctx_t* prev = (i2c_reg_ctx_t*)ctx;

    i2c_reg_ctx_t* next = (i2c_reg_ctx_t*)memrina_alloc(mu_session, sizeof(i2c_reg_ctx_t));
    if (!next) {
        return NULL;
    }

    next->i2c = prev->i2c;
    next->addr = (uint16_t)arg->value.integer;
    next->reg = 0;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = i2c_reg_read_reg;
    val->value.native.ctx = next;

    return val;
}

value_t* builtin_i2c_reg_read(value_t* arg) {
    if (!arg || arg->valueType != VAR_STRING) {
        BUILTIN_ERR("i2cRegRead");
    }

    const struct device* i2c = device_get_binding(arg->value.string);
    if (!i2c || !device_is_ready(i2c)) {
        printk("[!] Error: i2cRegRead device '%s' not ready\n", arg->value.string);
        return make_error();
    }

    i2c_reg_ctx_t* ctx = (i2c_reg_ctx_t*)memrina_alloc(mu_session, sizeof(i2c_reg_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->i2c = i2c;
    ctx->addr = 0;
    ctx->reg = 0;

    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return NULL;
    }

    val->valueType = VAR_NATIVE_CLOSURE;
    val->is_return = 0;
    val->refcount = 1;
    val->value.native.fn = i2c_reg_read_addr;
    val->value.native.ctx = ctx;

    return val;
}

void register_builtin(env_t* env, const char* name, builtin_fn fn) {
    value_t* val = (value_t*)memrina_alloc(mu_session, sizeof(value_t));
    if (!val) {
        return;
    }

    val->valueType = VAR_BUILTIN;
    val->is_return = 0;
    val->refcount = 1;
    val->value.builtin = fn;

    create_binding(env, (char*)name, strlen(name), val);
    value_release(val);
}
