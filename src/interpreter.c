#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

typedef enum value_type {
    VAR_INT,
    VAR_STRING,
    VAR_LIST,
    VAR_CLOSURE
}value_type_t;

typedef struct closure {
    ast_node_t* params;
    ast_node_t* body;
    // env_t* env; <- not implemented yet
    int tailcall;
}closure_t;

typedef struct value {
    value_type_t valueType;

    union {
        int integer;
        char* string;
        closure_t closure;
    };
}value_t;

value_t* evaluate(ast_node_t* node)
{
    value_t* value = (value_t*)malloc(sizeof(value_t));

    if (!value)
        return NULL;

    switch (node->type) {

        case NODE_BINOP:
           //value_t left = evaluate(node->left);
           //value_t right = evaluate(node->right);
            
        case NODE_INT:

    }
}
