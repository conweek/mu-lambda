#ifndef PARSER_H_
#define PARSER_H_

#include "tokeniser.h"

typedef enum node_type_t {
    NODE_INT,
    NODE_STR,
    NODE_VAR,
    NODE_BINOP,
    NODE_NEG,
    NODE_ASSIGN,
    NODE_IF,
    NODE_FN,
    NODE_LAMBDA,
    NODE_APPLY,
    NODE_RETURN,
    NODE_TAILCALL,
    NODE_BLOCK
}node_type_t;

typedef struct ASTNode {
    node_type_t type;
    token_t token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* cond;
}ast_node_t;

typedef struct Parser {
    token_t* tokens;
    int pos;
}parser_t;

parser_t parser_init(token_t* tokens);
token_t parser_current(parser_t* p);
token_t parser_advance(parser_t* p);
token_t parser_expect(parser_t* p, atomic_token_t type);
void parser_skip_newlines(parser_t* p);

ast_node_t* parse_program(parser_t* p);
ast_node_t* parse_block(parser_t* p);
ast_node_t* parse_statement(parser_t* p);
ast_node_t* parse_if(parser_t* p);
ast_node_t* parse_fn(parser_t* p, int tailcall);
ast_node_t* parse_lambda(parser_t* p);
ast_node_t* parse_expr_statement(parser_t* p);
ast_node_t* parse_comparison(parser_t* p);
ast_node_t* parse_term(parser_t* p);
ast_node_t* parse_call(parser_t* p);
ast_node_t* parse_atomic(parser_t* p);

#endif
