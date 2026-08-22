#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "tokeniser.h"
#include "parser.h"
#include "interpreter.h"
#include "builtins.h"

value_t* make_int(int val) {
    value_t* value = (value_t*)k_malloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_INT;
    value->value.integer = val;
    value->is_return = 0;
    value->refcount = 1;

    return value;
}

value_t* make_no_result() {
    value_t* value = (value_t*)k_malloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_NORESULT;
    value->is_return = 0;
    value->refcount = 1;

    return value;
}

value_t* make_error() {
    value_t* value = (value_t*)k_malloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_ERROR;
    value->is_return = 0;
    value->refcount = 1;

    return value;
}

static inline int is_int(value_t* node) {
    return (node->valueType == VAR_INT);
}

static inline int is_str(value_t* node) {
    return (node->valueType == VAR_STRING);
}

value_t* convert_value(node_type_t type, char* val, int len) {
    value_t* value = (value_t*)k_malloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_UNKNOWN;
    value->is_return = 0;
    value->refcount = 1;

    switch (type) {
    case NODE_INT: {
        char buf[32];
        int n = len < 31 ? len : 31;
        memcpy(buf, val, n);
        buf[n] = '\0';
        value->valueType = VAR_INT;
        value->value.integer = strtol(buf, NULL, 0);
        break;
    }
    case NODE_STR: {
        char* s = k_malloc(len + 1);
        memcpy(s, val, len);
        s[len] = '\0';
        value->valueType = VAR_STRING;
        value->value.string = s;
        break;
    }
    default:
        k_free(value);
        return NULL;
    }

    return value;
}

env_t* create_env(env_t* parent) {
    env_t* env = (env_t*)k_malloc(sizeof(env_t));

    if (!env) {
        return NULL;
    }

    env->parent = parent;
    env->bindings = NULL;
    env->count = 0;
    env->refcount = 1;

    if (parent) {
        env_retain(parent);
    }

    return env;
}

void env_retain(env_t* env) {
    if (env) {
        env->refcount++;
    }
}

void env_release(env_t* env) {
    if (!env) {
        return;
    }

    env->refcount--;
    if (env->refcount > 0) {
        return;
    }

    binding_t* b = env->bindings;
    while (b) {
        binding_t* next = b->next;
        value_release(b->value);
        k_free(b->name);
        k_free(b);
        b = next;
    }

    env_release(env->parent);
    k_free(env);
}

void value_retain(value_t* val) {
    if (val) {
        val->refcount++;
    }
}

void value_release(value_t* val) {
    if (!val) {
        return;
    }

    val->refcount--;
    if (val->refcount > 0) {
        return;
    }

    if (val->valueType == VAR_STRING) {
        k_free(val->value.string);
    } else if (val->valueType == VAR_CLOSURE) {
        env_release(val->value.closure.env);
    } else if (val->valueType == VAR_NATIVE_CLOSURE) {
        k_free(val->value.native.ctx);
    } else if (val->valueType == VAR_THUNK) {
        value_release(val->value.thunk.fn);
        value_release(val->value.thunk.arg);
    }

    k_free(val);
}

int create_binding(env_t* env, char* name, int nameLen, value_t* value) {
    binding_t* binding = k_malloc(sizeof(binding_t));

    if (!binding) {
        return MU_BINDING_ERR;
    }

    binding->name = k_malloc(nameLen + 1);
    if (!binding->name) {
        k_free(binding);
        return MU_BINDING_ERR;
    }
    memcpy(binding->name, name, nameLen);
    binding->name[nameLen] = '\0';

    binding->value = value;
    value_retain(value);
    binding->next = NULL;

    if (env->bindings == NULL) {
        env->bindings = binding;
    } else {
        binding_t* curr = env->bindings;

        while (curr->next != NULL) {
            curr = curr->next;
        }

        curr->next = binding;
    }

    env->count++;
    return MU_SUCCESS;
}

value_t* env_lookup(env_t* env, char* name, int nameLen) {
    binding_t* binding = env->bindings;

    if (env->bindings == NULL) {
        if (env->parent == NULL) {
            return NULL;
        }

        return env_lookup(env->parent, name, nameLen);
    }

    while (binding != NULL) {
        if (strncmp(binding->name, name, nameLen) == 0 && binding->name[nameLen] == '\0') {
            return binding->value;
        }

        binding = binding->next;
    }

    if (env->parent != NULL) {
        return env_lookup(env->parent, name, nameLen);
    }

    return NULL;
}

static value_t* make_thunk(value_t* fn, value_t* arg) {
    value_t* thunk = (value_t*)k_malloc(sizeof(value_t));

    if (!thunk) {
        return NULL;
    }

    thunk->valueType = VAR_THUNK;
    thunk->is_return = 1;
    thunk->refcount = 1;
    thunk->value.thunk.fn = fn;
    thunk->value.thunk.arg = arg;
    value_retain(fn);
    value_retain(arg);

    return thunk;
}

value_t* run_interpreter(char* source) {
    char* ptr = source;
    token_t* tokens = get_token_list(&ptr);
    if (!tokens) {
        printk("[!] Error: failed to allocate token list\n");
        return NULL;
    }

    parser_t p = parser_init(tokens);
    ast_node_t* ast = parse_program(&p);

    if (p.error) {
        ast_free(ast);
        k_free(tokens);
        return NULL;
    }

    env_t* env = create_env(NULL);
    if (!env) {
        printk("[!] Error: failed to create environment\n");
        ast_free(ast);
        k_free(tokens);
        return NULL;
    }

    register_builtin(env, "print", builtin_print);
    register_builtin(env, "sleep", builtin_sleep);
    register_builtin(env, "gpioSet", builtin_gpio_set);
    register_builtin(env, "gpioRead", builtin_gpio_read);

    value_t* result = evaluate(ast, env);

    env_release(env);
    ast_free(ast);
    k_free(tokens);

    if (!result) {
        printk("[!] Error: evaluation failed\n");
        return NULL;
    }

    return result;
}

value_t* evaluate(ast_node_t* node, env_t* env) {
    return evaluate_tc(node, env, 0);
}

value_t* evaluate_tc(ast_node_t* node, env_t* env, int in_tailcall) {
    switch (node->type) {
    case NODE_ERROR:
        return NULL;
    case NODE_INT:
        return convert_value(NODE_INT, node->token.str, node->token.len);
    case NODE_STR:
        return convert_value(NODE_STR, node->token.str, node->token.len);

        SCOPED_CASE(NODE_VAR)
        value_t* val = env_lookup(env, node->token.str, node->token.len);
        if (!val) {
            printk("[!] Error: undefined variable '%.*s'\n", node->token.len, node->token.str);
            return NULL;
        }
        value_retain(val);
        return val;
        END_SCOPE

        SCOPED_CASE(NODE_BINOP)
        value_t* left = evaluate_tc(node->left, env, 0);
        value_t* right = evaluate_tc(node->right, env, 0);

        if (!left || !right) {
            value_release(left);
            value_release(right);
            return NULL;
        }

        if (left->valueType != VAR_INT || right->valueType != VAR_INT) {
            printk("[!] Error: binary operator requires integer operands\n");
            value_release(left);
            value_release(right);
            return NULL;
        }

        int l = left->value.integer;
        int r = right->value.integer;
        value_release(left);
        value_release(right);

        switch (node->token.token) {
        case TOKEN_PLUS:
            return make_int(l + r);
        case TOKEN_MINUS:
            return make_int(l - r);
        case TOKEN_EQUALTO:
            return make_int(l == r);
        case TOKEN_NOTEQUALTO:
            return make_int(l != r);
        case TOKEN_GREATERTHAN:
            return make_int(l > r);
        case TOKEN_LESSTHAN:
            return make_int(l < r);
        case TOKEN_TIMES:
            return make_int(l * r);
        case TOKEN_DIVIDE:
            if (r == 0) {
                printk("[!] Error: division by zero\n");
                return NULL;
            }
            return make_int(l / r);
        default:
            return NULL;
        }
        END_SCOPE

        SCOPED_CASE(NODE_NEG)
        value_t* left = evaluate_tc(node->left, env, 0);

        if (!left) {
            return NULL;
        }

        if (left->valueType != VAR_INT) {
            printk("[!] Error: negation requires integer operand\n");
            value_release(left);
            return NULL;
        }

        left->value.integer = -(left->value.integer);
        return left;
        END_SCOPE

        SCOPED_CASE(NODE_ASSIGN)
        if (env_lookup(env, node->token.str, node->token.len) != NULL) {
            printk("[!] Error: variable '%.*s' already defined\n", node->token.len,
                   node->token.str);
            return NULL;
        }

        value_t* left = evaluate_tc(node->left, env, 0);
        if (!left) {
            return NULL;
        }

        if (create_binding(env, node->token.str, node->token.len, left) != MU_SUCCESS) {
            value_release(left);
            return NULL;
        }

        return left;
        END_SCOPE

        SCOPED_CASE(NODE_IF)
        value_t* condition = evaluate_tc(node->cond, env, 0);

        if (!condition) {
            return NULL;
        }

        if (is_int(condition) && condition->value.integer != 0) {
            env_t* child = create_env(env);
            value_t* result = evaluate_tc(node->left, child, in_tailcall);
            value_release(condition);
            env_release(child);
            return result;
        } else if (node->right != NULL) {
            env_t* child = create_env(env);
            value_t* result = evaluate_tc(node->right, child, in_tailcall);
            value_release(condition);
            env_release(child);
            return result;
        }

        value_release(condition);
        return make_no_result();
        END_SCOPE

        SCOPED_CASE(NODE_FN)
        value_t* func = (value_t*)k_malloc(sizeof(value_t));
        func->valueType = VAR_CLOSURE;
        func->is_return = 0;
        func->refcount = 1;
        func->value.closure.env = env;
        env_retain(env);
        func->value.closure.tailcall = 0;
        func->value.closure.body = node->right;
        func->value.closure.params = node->left;
        create_binding(env, node->token.str, node->token.len, func);
        value_release(func);
        return make_no_result();
        END_SCOPE

        SCOPED_CASE(NODE_LAMBDA)
        value_t* fn = k_malloc(sizeof(value_t));
        fn->valueType = VAR_CLOSURE;
        fn->is_return = 0;
        fn->refcount = 1;
        fn->value.closure.params = node->left;
        fn->value.closure.body = node->right;
        fn->value.closure.env = env;
        env_retain(env);
        fn->value.closure.tailcall = 0;
        return fn;
        END_SCOPE

        SCOPED_CASE(NODE_APPLY)
        value_t* fn = evaluate_tc(node->left, env, 0);
        value_t* arg = evaluate_tc(node->right, env, 0);

        if (!fn || !arg) {
            value_release(fn);
            value_release(arg);
            return NULL;
        }

        if (fn->valueType == VAR_BUILTIN) {
            value_t* result = fn->value.builtin(arg);
            value_release(fn);
            value_release(arg);
            if (result && result->valueType == VAR_ERROR) {
                value_release(result);
                return NULL;
            }
            return result;
        }

        if (fn->valueType == VAR_NATIVE_CLOSURE) {
            value_t* result = fn->value.native.fn(fn->value.native.ctx, arg);
            value_release(fn);
            value_release(arg);
            if (result && result->valueType == VAR_ERROR) {
                value_release(result);
                return NULL;
            }
            return result;
        }

        if (fn->valueType != VAR_CLOSURE) {
            printk("[!] Error: attempt to call a non-function value\n");
            value_release(fn);
            value_release(arg);
            return NULL;
        }

        if (in_tailcall) {
            value_t* thunk = make_thunk(fn, arg);
            value_release(fn);
            value_release(arg);
            return thunk;
        }

        for (;;) {
            closure_t* cl = &fn->value.closure;
            env_t* call_env = create_env(cl->env);

            ast_node_t* param = cl->params;
            create_binding(call_env, param->token.str, param->token.len, arg);
            value_release(arg);

            if (param->right != NULL) {
                value_t* partial = k_malloc(sizeof(value_t));
                partial->valueType = VAR_CLOSURE;
                partial->is_return = 0;
                partial->refcount = 1;
                partial->value.closure.params = param->right;
                partial->value.closure.body = cl->body;
                partial->value.closure.env = call_env;
                env_retain(call_env);
                partial->value.closure.tailcall = cl->tailcall;
                value_release(fn);
                env_release(call_env);
                return partial;
            }

            value_t* result = evaluate_tc(cl->body, call_env, cl->tailcall);

            if (result) {
                result->is_return = 0;
            }

            value_release(fn);
            env_release(call_env);

            if (result && result->valueType == VAR_THUNK) {
                fn = result->value.thunk.fn;
                arg = result->value.thunk.arg;
                value_retain(fn);
                value_retain(arg);
                value_release(result);
                continue;
            }

            return result;
        }
        END_SCOPE

        SCOPED_CASE(NODE_RETURN)
        value_t* value = evaluate_tc(node->left, env, in_tailcall);

        if (!value) {
            return NULL;
        }

        value->is_return = 1;
        return value;
        END_SCOPE

        SCOPED_CASE(NODE_TAILCALL)
        value_t* func = (value_t*)k_malloc(sizeof(value_t));
        func->valueType = VAR_CLOSURE;
        func->is_return = 0;
        func->refcount = 1;
        func->value.closure.env = env;
        env_retain(env);
        func->value.closure.tailcall = 1;
        func->value.closure.body = node->right;
        func->value.closure.params = node->left;
        create_binding(env, node->token.str, node->token.len, func);
        value_release(func);
        return make_no_result();
        END_SCOPE

        SCOPED_CASE(NODE_BLOCK)
        value_t* result = NULL;
        ast_node_t* curr = node;

        while (curr != NULL) {
            if (curr->type != NODE_BLOCK) {
                value_release(result);
                result = evaluate_tc(curr, env, in_tailcall);
                break;
            }

            value_release(result);
            result = evaluate_tc(curr->left, env, in_tailcall);

            if (result && result->is_return) {
                return result;
            }

            curr = curr->right;
        }

        return result;
        END_SCOPE

        SCOPED_CASE(NODE_ENTRY)
        evaluate_tc(node->left, env, 0);

        value_t* fn = env_lookup(env, node->left->token.str, node->left->token.len);
        if (!fn || fn->valueType != VAR_CLOSURE) {
            return NULL;
        }

        value_retain(fn);
        closure_t* cl = &fn->value.closure;

        if (cl->params != NULL) {
            printk("[!] Error: entry point function must take no arguments\n");
            value_release(fn);
            return NULL;
        }

        for (;;) {
            env_t* call_env = create_env(cl->env);
            value_t* result = evaluate_tc(cl->body, call_env, cl->tailcall);
            if (result) {
                result->is_return = 0;
            }
            env_release(call_env);

            if (result == fn) {
                value_release(result);
                continue;
            }

            value_release(fn);
            return result;
        }
        END_SCOPE
    }

    return NULL;
}
