#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

// Creates a new node
static ast_node_t* make_node(node_type_t type, token_t token, ast_node_t* left, ast_node_t* right)
{
    // Malloc to be replaced with memory arena
    ast_node_t* node = (ast_node_t*)malloc(sizeof(ast_node_t));

    if (!node) 
        return NULL;

    node->type = type;
    node->token = token;
    node->left = left;
    node->right = right;
    node->cond = NULL;

    return node;
}

// Returns whether a token is atomic (int, str, list, identifier, openparen)
static int is_atomic_start(atomic_token_t t)
{
    return (t == TOKEN_INT || t == TOKEN_STR || t == TOKEN_LIST || t == TOKEN_IDENTIFIER || t == TOKEN_OPENPAREN);
}

// Creates a new parser instance
parser_t parser_init(token_t* tokens)
{

    parser_t p = { 
        .tokens = tokens, 
        .pos = 0 
    };

    return p;
}

// Retrieves current token
token_t parser_current(parser_t* p)
{
    return p->tokens[p->pos];
}

// Retrieves next token in sequence
token_t parser_advance(parser_t* p)
{
    return p->tokens[p->pos++];
}

// Asserts token is what is expected then advances to the next token
token_t parser_expect(parser_t* p, atomic_token_t type)
{
    token_t tok = parser_current(p);

    // TODO: Change to printk/LOGERR for Zephyr
    if (tok.token != type) {
        fprintf(stderr, "parse error: expected token %d, got %d\n", type, tok.token);
        exit(1);
    }

    return parser_advance(p);
}

// Skips all newline tokens
void parser_skip_newlines(parser_t* p)
{
    while (parser_current(p).token == TOKEN_NEWLINE)
        parser_advance(p);
}

// program → (statement NEWLINE*)* EOF
// Parses a whole program
ast_node_t* parse_program(parser_t* p)
{
    ast_node_t* root = NULL;
    ast_node_t* tail = NULL;

    parser_skip_newlines(p);

    while (parser_current(p).token != TOKEN_EOF) {

        if (parser_current(p).token == TOKEN_COMMENT) {
            parser_advance(p);
            parser_skip_newlines(p);
            continue;
        }

        ast_node_t* stmt = parse_statement(p);
        ast_node_t* block = make_node(NODE_BLOCK, parser_current(p), stmt, NULL);

        if (!root) {
            root = block;
            tail = block;
        } else {
            tail->right = block;
            tail = block;
        }

        parser_skip_newlines(p);
    }

    return root;
}

// block → COLON NEWLINE* statement (NEWLINE+ statement)* NEWLINE* END
// Parses block of statements
ast_node_t* parse_block(parser_t* p)
{
    parser_expect(p, TOKEN_COLON);
    parser_skip_newlines(p);

    ast_node_t* root = NULL;
    ast_node_t* tail = NULL;

    while (parser_current(p).token != TOKEN_END) {

        if (parser_current(p).token == TOKEN_COMMENT) {
            parser_advance(p);
            parser_skip_newlines(p);
            continue;
        }

        ast_node_t* stmt = parse_statement(p);
        ast_node_t* block = make_node(NODE_BLOCK, parser_current(p), stmt, NULL);

        if (!root) {
            root = block;
            tail = block;
        } else {
            tail->right = block;
            tail = block;
        }

        parser_skip_newlines(p);
    }

    parser_expect(p, TOKEN_END);
    return root;
}

// statement → fn | ts | if | return | assignment | expr
// Parses a statement of some kind
ast_node_t* parse_statement(parser_t* p)
{
    token_t tok = parser_current(p);

    switch (tok.token) {
        case TOKEN_IF:
            return parse_if(p);
        case TOKEN_FUNCTION:
            return parse_fn(p, 0);
        case TOKEN_TAILCALL:
            return parse_fn(p, 1);
        case TOKEN_RETURN:
            parser_advance(p);
            ast_node_t* val = parse_expr(p);
            return make_node(NODE_RETURN, tok, val, NULL);
        case TOKEN_IDENTIFIER:
            if (p->tokens[p->pos + 1].token == TOKEN_ASSIGNMENT) {
                token_t name = parser_advance(p);
                parser_advance(p);
                ast_node_t* val = parse_expr(p);
                return make_node(NODE_ASSIGN, name, val, NULL);
            }
            return parse_expr(p);
        default:
            return parse_expr(p);
    }
}

// if → IF expr COLON statements (ELSE COLON statements)? END
// Parses if/else statements
ast_node_t* parse_if(parser_t* p)
{
    token_t tok = parser_expect(p, TOKEN_IF);
    ast_node_t* cond = parse_expr(p);
    parser_expect(p, TOKEN_COLON);
    parser_skip_newlines(p);

    ast_node_t* then_root = NULL;
    ast_node_t* then_tail = NULL;

    while (parser_current(p).token != TOKEN_ELSE && parser_current(p).token != TOKEN_END) {
        ast_node_t* stmt = parse_statement(p);
        ast_node_t* blk = make_node(NODE_BLOCK, parser_current(p), stmt, NULL);
        
        if (!then_root) { 
            then_root = blk; 
            then_tail = blk; 
        } else { 
            then_tail->right = blk; 
            then_tail = blk; 
        }

        parser_skip_newlines(p);
    }

    ast_node_t* else_branch = NULL;

    if (parser_current(p).token == TOKEN_ELSE) {
        parser_advance(p);
        parser_expect(p, TOKEN_COLON);
        parser_skip_newlines(p);

        ast_node_t* else_root = NULL;
        ast_node_t* else_tail = NULL;

        while (parser_current(p).token != TOKEN_END) {
            ast_node_t* stmt = parse_statement(p);
            ast_node_t* blk = make_node(NODE_BLOCK, parser_current(p), stmt, NULL);
            
            if (!else_root) { 
                else_root = blk; 
                else_tail = blk; 
            } else { 
                else_tail->right = blk; 
                else_tail = blk; 
            }

            parser_skip_newlines(p);
        }

        else_branch = else_root;
    }

    parser_expect(p, TOKEN_END);

    ast_node_t* node = make_node(NODE_IF, tok, then_root, else_branch);
    node->cond = cond;
    return node;
}

// fn  → FN IDENTIFIER ARROW params block
// ts  → TC FN IDENTIFIER ARROW params block
// Parses both regular and tail recursive functions
ast_node_t* parse_fn(parser_t* p, int tailcall)
{
    token_t tok;
    if (tailcall)
        parser_expect(p, TOKEN_TAILCALL);

    parser_expect(p, TOKEN_FUNCTION);
    token_t name = parser_expect(p, TOKEN_IDENTIFIER);
    parser_expect(p, TOKEN_ARROW);

    // Params until COLON
    ast_node_t* params = NULL;
    ast_node_t* ptail  = NULL;

    while (parser_current(p).token == TOKEN_IDENTIFIER) {
        tok = parser_advance(p);
        ast_node_t* pnode = make_node(NODE_VAR, tok, NULL, NULL);
        if (!params) {
            params = pnode;
            ptail  = pnode;
        } else {
            ptail->right = pnode;
            ptail = pnode;
        }
    }

    ast_node_t* body = parse_block(p);
    node_type_t type = tailcall ? NODE_TAILCALL : NODE_FN;
    ast_node_t* node = make_node(type, name, params, body);
    return node;
}

// lambda → LAMBDA params ARROW expr
// Parses a lambda function into its components
ast_node_t* parse_lambda(parser_t* p)
{
    token_t tok = parser_expect(p, TOKEN_LAMBDA);

    ast_node_t* params = NULL;
    ast_node_t* ptail  = NULL;

    while (parser_current(p).token == TOKEN_IDENTIFIER) {
        token_t param = parser_advance(p);
        ast_node_t* pnode = make_node(NODE_VAR, param, NULL, NULL);
        
        if (!params) {
            params = pnode;
            ptail  = pnode;
        } else {
            ptail->right = pnode;
            ptail = pnode;
        }

    }

    parser_expect(p, TOKEN_ARROW);
    ast_node_t* body = parse_expr(p);
    return make_node(NODE_LAMBDA, tok, params, body);
}

// expr → lambda | dollar
// Parses symbols \ and $
ast_node_t* parse_expr(parser_t* p)
{
    if (parser_current(p).token == TOKEN_LAMBDA)
        return parse_lambda(p);

    return parse_dollar(p);
}

// dollar → comparison ($ dollar)?
ast_node_t* parse_dollar(parser_t* p)
{
    ast_node_t* left = parse_comparison(p);

    if (parser_current(p).token == TOKEN_DOLLARSIGN) {
        token_t tok = parser_advance(p);
        ast_node_t* right = parse_dollar(p);
        return make_node(NODE_DOLLAR, tok, left, right);
    }

    return left;
}

// comparison → addition ((== | != | > | <) addition)?
// Handles function result comparison function result
// Handles atomic comparison atomic
ast_node_t* parse_comparison(parser_t* p)
{
    ast_node_t* left = parse_addition(p);
    token_t tok = parser_current(p);

    if (tok.token == TOKEN_EQUALTO || tok.token == TOKEN_NOTEQUALTO || tok.token == TOKEN_GREATERTHAN || tok.token == TOKEN_LESSTHAN) {
        parser_advance(p);
        ast_node_t* right = parse_addition(p);
        left = make_node(NODE_BINOP, tok, left, right);
    }

    return left;
}

// addition → application ((+ | -) application)*
// Handles function result +- function result
// Handles atomic +- atomic
ast_node_t* parse_addition(parser_t* p)
{
    ast_node_t* left = parse_application(p);

    while (parser_current(p).token == TOKEN_PLUS || parser_current(p).token == TOKEN_MINUS) {
        token_t op = parser_advance(p);
        ast_node_t* right = parse_application(p);
        left = make_node(NODE_BINOP, op, left, right);
    }

    return left;
}

// application → atomic atomic*
// Parses function/identifier calls with another atomic
// is LEFT ASSOCIATIVE application
ast_node_t* parse_application(parser_t* p)
{
    ast_node_t* left = parse_atomic(p);

    while (is_atomic_start(parser_current(p).token)) {
        ast_node_t* arg = parse_atomic(p);
        left = make_node(NODE_APPLY, left->token, left, arg);
    }

    return left;
}

// atomic → INT | STR | LIST | IDENTIFIER | ( expr )
// Parses an atomic token
ast_node_t* parse_atomic(parser_t* p)
{
    token_t tok = parser_current(p);

    switch (tok.token) {
        case TOKEN_INT:
            parser_advance(p);
            return make_node(NODE_INT, tok, NULL, NULL);
        case TOKEN_STR:
            parser_advance(p);
            return make_node(NODE_STR, tok, NULL, NULL);
        case TOKEN_LIST:
            parser_advance(p);
            return make_node(NODE_LIST, tok, NULL, NULL);
        case TOKEN_IDENTIFIER:
            parser_advance(p);
            return make_node(NODE_VAR, tok, NULL, NULL);
        case TOKEN_OPENPAREN:
            parser_advance(p);
            ast_node_t* inner = parse_expr(p);
            parser_expect(p, TOKEN_CLOSEPAREN);
            return inner; 
        // TODO: Change to printk/LOGERR for Zephyr
        default:
            fprintf(stderr, "parse error: unexpected token %d\n", tok.token);
            exit(1);
    }
}
