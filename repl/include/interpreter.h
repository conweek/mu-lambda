#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "parser.h"

#define BINDING_ERR -1
#define SUCCESS 0
#define SCOPED_CASE(name) case name: {
#define END_SCOPE }

typedef enum value_type {
    VAR_UNKNOWN,
    VAR_INT,
    VAR_STRING,
    VAR_LIST,
    VAR_NORESULT,
    VAR_CLOSURE,
    VAR_BUILTIN,
    VAR_THUNK
}value_type_t;

typedef struct value value_t;
typedef struct binding binding_t;
typedef value_t* (*builtin_fn)(value_t* arg);

struct binding {
    char* name;
    value_t* value;
    binding_t* next;
};

typedef struct env {
    struct env* parent;
    binding_t* bindings;
    int count;
    int refcount;
} env_t;

typedef struct closure {
    ast_node_t* params;
    ast_node_t* body;
    env_t* env;
    int tailcall;
}closure_t;

typedef struct thunk {
    value_t* fn;
    value_t* arg;
}thunk_t;

struct value {
    value_type_t valueType;
    int is_return;
    int refcount;
    union {
        int integer;
        char* string;
        closure_t closure;
        thunk_t thunk;
        builtin_fn builtin;
    }value;
};

env_t* create_env(env_t* parent);
void env_retain(env_t* env);
void env_release(env_t* env);
void value_retain(value_t* val);
void value_release(value_t* val);
int create_binding(env_t* env, char* name, int nameLen, value_t* value);
value_t* env_lookup(env_t* env, char* name, int nameLen);
value_t* evaluate(ast_node_t* node, env_t* env);
value_t* evaluate_tc(ast_node_t* node, env_t* env, int in_tailcall);
void register_builtin(env_t* env, const char* name, builtin_fn fn);
value_t* run_interpreter(char* source);

#endif
