#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"
#include "tokeniser.h"

// Retrieves current token
static inline token_t parser_current(parser_t* p) {
    return p->tokens[p->pos];
}

// Retrieves next token in sequence
static inline token_t parser_advance(parser_t* p) {
    return p->tokens[p->pos++];
}

// Creates a new parser instance
parser_t parser_init(token_t* tokens) {
    parser_t p = {.tokens = tokens, .pos = 0};
    return p;
}

// Check if the next token is the type requested
static bool parser_is_match(parser_t* p, atomic_token_t type) {
    token_t tok = parser_current(p);
    if (tok.token != type) {

        return false;
    }

    return true;
}

// Asserts token is what is expected then advances to the next token
static token_t parser_match(parser_t* p, atomic_token_t type) {
    if (parser_is_match(p, type)) {
        return parser_advance(p);
    }

    fprintf(stderr, "parse error: expected token %d, got %d\n", type, parser_current(p).token);
    exit(1);
}

// Skips all newline tokens
void parser_skip_newline(parser_t* p) {
    while (parser_current(p).token == TOKEN_NEWLINE) {
        parser_advance(p);
    }
}

// Creates a new node
static ast_node_t* make_node(node_type_t type, token_t token, ast_node_t* left, ast_node_t* right) {
    // Malloc to be replaced with memory arena
    ast_node_t* node = (ast_node_t*)malloc(sizeof(ast_node_t));

    if (!node) {
        return NULL;
    }

    node->type = type;
    node->token = token;
    node->left = left;
    node->right = right;
    node->cond = NULL;

    return node;
}

// Returns whether a token is atomic (int, str, identifier, openparen)
static int is_atomic_start(atomic_token_t t) {
    return (t == TOKEN_INT || t == TOKEN_STR || t == TOKEN_IDENTIFIER || t == TOKEN_OPENPAREN);
}

// Returns whether a comparison operator token follows
static int is_op(atomic_token_t t) {
    return (t == TOKEN_EQUALTO || t == TOKEN_NOTEQUALTO || t == TOKEN_GREATERTHAN ||
            t == TOKEN_LESSTHAN);
}

// Returns whether the parser is positioned at a lambda literal: "(" "\"
static int is_lambda_start(parser_t* p) {
    return (parser_current(p).token == TOKEN_OPENPAREN &&
            p->tokens[p->pos + 1].token == TOKEN_LAMBDA);
}

// program = { statement NEWLINE } [ EP ( tsStatement | fnStatement ) NEWLINE ] EOF
ast_node_t* parse_program(parser_t* p) {
    ast_node_t* root = NULL;
    ast_node_t* tail = NULL;
    ast_node_t* block = NULL;

    while (parser_current(p).token != TOKEN_EOF && parser_current(p).token != TOKEN_ENTRYPOINT) {

        switch (parser_current(p).token) {
        case TOKEN_COMMENT:
            parser_match(p, TOKEN_COMMENT);
            break;
        default:
            block = make_node(NODE_BLOCK, parser_current(p), parse_statement(p), NULL);
            if (!root) {
                root = block;
                tail = block;
            } else {
                tail->right = block;
                tail = block;
            }
        }

        parser_skip_newline(p);
    }

    if (parser_is_match(p, TOKEN_ENTRYPOINT)) {
        token_t tok = parser_match(p, TOKEN_ENTRYPOINT); // skip over the Key word EP
        ast_node_t* entry;
        switch (parser_current(p).token) {
        case TOKEN_TAILCALL:
            entry = parse_fn(p, 1);
            break;
        case TOKEN_FUNCTION:
            entry = parse_fn(p, 0);
            break;
        default:
            // TODO: Change to printk/LOGERR for Zephyr
            fprintf(stderr,
                    "parse error: EP must be followed by a function definition, got token %d\n",
                    parser_current(p).token);
            exit(1);
        }

        ast_node_t* entry_point = make_node(NODE_ENTRY, tok, entry, NULL);
        if (!root) {
            root = entry_point;
        } else {
            tail->right = entry_point;
        }

        parser_skip_newline(p);
    }

    parser_match(p, TOKEN_EOF);
    return root;
}

// block = statement NEWLINE { statement NEWLINE } END
ast_node_t* parse_block(parser_t* p) {
    ast_node_t* root = NULL;
    ast_node_t* tail = NULL;
    ast_node_t* block = NULL;

    while (!parser_is_match(p, TOKEN_END)) {
        switch (parser_current(p).token) {
        case TOKEN_COMMENT:
            parser_match(p, TOKEN_COMMENT);
            break;
        default:
            block = make_node(NODE_BLOCK, parser_current(p), parse_statement(p), NULL);
            if (!root) {
                root = block;
                tail = block;
            } else {
                tail->right = block;
                tail = block;
            }
        }

        parser_skip_newline(p);
    }

    parser_match(p, TOKEN_END);
    return root;
}

// statement = fnStatement | tsStatement | ifStatement | returnStatement | assignmentStatement
ast_node_t* parse_statement(parser_t* p) {
    token_t tok;
    switch (parser_current(p).token) {
    case TOKEN_IF:
        return parse_if(p);
    case TOKEN_FUNCTION:
        return parse_fn(p, 0);
    case TOKEN_TAILCALL:
        return parse_fn(p, 1);
    case TOKEN_RETURN:
        return make_node(NODE_RETURN, parser_match(p, TOKEN_RETURN), parse_expr_statement(p), NULL);
    case TOKEN_IDENTIFIER:
        tok = parser_match(p, TOKEN_IDENTIFIER); // grab the left hand side
        if (parser_is_match(p, TOKEN_ASSIGNMENT)) {
            parser_match(p, TOKEN_ASSIGNMENT); // skip over the '='
            return make_node(NODE_ASSIGN, tok, parse_expr_statement(p), NULL);
        } else {
            // a bare identifier is not a valid statement
            // TODO: Change to printk/LOGERR for Zephyr
            fprintf(stderr, "parse error: unexpected token %d, expected a statement\n", tok.token);
            exit(1);
        }
        break;
    default:
        // TODO: Change to printk/LOGERR for Zephyr
        fprintf(stderr, "parse error: unexpected token %d, expected a statement\n", tok.token);
        exit(1);
    }
}

// ifStatement = IF exprStatement COLON NEWLINE { statement NEWLINE } [ ELSE COLON NEWLINE {
// statement NEWLINE } ] END
ast_node_t* parse_if(parser_t* p) {
    token_t tok = parser_match(p, TOKEN_IF);
    ast_node_t* cond = parse_expr_statement(p);
    parser_match(p, TOKEN_COLON);
    parser_skip_newline(p);

    ast_node_t* then_root = NULL;
    ast_node_t* then_tail = NULL;

    while (!(parser_is_match(p, TOKEN_ELSE) || parser_is_match(p, TOKEN_END))) {
        // The true block
        ast_node_t* blk = make_node(NODE_BLOCK, parser_current(p), parse_statement(p), NULL);

        if (!then_root) {
            then_root = blk;
            then_tail = blk;
        } else {
            then_tail->right = blk;
            then_tail = blk;
        }

        parser_skip_newline(p);
    }

    ast_node_t* else_branch = NULL;

    if (parser_is_match(p, TOKEN_ELSE)) {
        // The False block
        parser_match(p, TOKEN_ELSE);
        parser_match(p, TOKEN_COLON);
        parser_skip_newline(p);

        ast_node_t* else_root = NULL;
        ast_node_t* else_tail = NULL;

        while (!parser_is_match(p, TOKEN_END)) {
            ast_node_t* blk = make_node(NODE_BLOCK, parser_current(p), parse_statement(p), NULL);
            if (!else_root) {
                else_root = blk;
                else_tail = blk;
            } else {
                else_tail->right = blk;
                else_tail = blk;
            }

            parser_skip_newline(p);
        }

        else_branch = else_root;
    }

    parser_match(p, TOKEN_END);

    ast_node_t* node = make_node(NODE_IF, tok, then_root, else_branch);
    node->cond = cond;
    return node;
}

// tsStatement = TS fnStatement
// fnStatement = FN IDENTIFIER ARROW { IDENTIFIER } COLON NEWLINE block
ast_node_t* parse_fn(parser_t* p, int tailcall) {
    if (tailcall) {
        parser_match(p, TOKEN_TAILCALL);
    }

    parser_match(p, TOKEN_FUNCTION);
    token_t name = parser_match(p, TOKEN_IDENTIFIER);
    parser_match(p, TOKEN_ARROW);

    // Params until COLON
    ast_node_t* params = NULL;
    ast_node_t* ptail = NULL;

    while (parser_is_match(p, TOKEN_IDENTIFIER)) {
        ast_node_t* pnode = make_node(NODE_VAR, parser_match(p, TOKEN_IDENTIFIER), NULL, NULL);
        if (!params) {
            params = pnode;
            ptail = pnode;
        } else {
            ptail->right = pnode;
            ptail = pnode;
        }
    }

    parser_match(p, TOKEN_COLON);
    parser_skip_newline(p);
    ast_node_t* node = make_node(tailcall ? NODE_TAILCALL : NODE_FN, name, params, parse_block(p));
    return node;
}

// lambda = OPENPAREN LAMBDA IDENTIFIER { IDENTIFIER } ARROW exprStatement CLOSEPAREN [ atomic {
// atomic } ]
ast_node_t* parse_lambda(parser_t* p) {
    parser_match(p, TOKEN_OPENPAREN);
    token_t tok = parser_match(p, TOKEN_LAMBDA);
    ast_node_t* params = make_node(NODE_VAR, parser_match(p, TOKEN_IDENTIFIER), NULL, NULL);
    ast_node_t* ptail = params;

    while (parser_is_match(p, TOKEN_IDENTIFIER)) {
        ast_node_t* pnode = make_node(NODE_VAR, parser_match(p, TOKEN_IDENTIFIER), NULL, NULL);
        ptail->right = pnode;
        ptail = pnode;
    }

    parser_match(p, TOKEN_ARROW);
    ast_node_t* body = parse_expr_statement(p);
    parser_match(p, TOKEN_CLOSEPAREN);

    ast_node_t* node = make_node(NODE_LAMBDA, tok, params, body);

    while (is_atomic_start(parser_current(p).token)) {
        ast_node_t* arg = parse_atomic(p);
        node = make_node(NODE_APPLY, tok, node, arg);
    }

    return node;
}

// exprStatement = comparison | lambda | term
// Parses an expression statement
ast_node_t* parse_expr_statement(parser_t* p) {
    return parse_comparison(p);
}

// comparison = (lambda | term) op (lambda | term)
// comparison's left-hand side is a (lambda | term), which is exactly
// exprStatement's lambda/term fallthrough when no operator follows, so both
// rules are implemented together here.
ast_node_t* parse_comparison(parser_t* p) {
    ast_node_t* left = is_lambda_start(p) ? parse_lambda(p) : parse_term(p);
    token_t tok = parser_current(p);

    if (is_op(tok.token)) {
        parser_advance(p);
        ast_node_t* right = is_lambda_start(p) ? parse_lambda(p) : parse_term(p);
        left = make_node(NODE_BINOP, tok, left, right);
    }

    return left;
}

// term = [ MINUS ] ( call | atomic ) { ( PLUS | MINUS ) ( call | atomic ) }
// call always takes priority over atomic's IDENTIFIER form when the
// lookahead is IDENTIFIER: call = IDENTIFIER { atomic } already covers the
// zero-argument (bare variable) case, so atomic's IDENTIFIER form is never
// tried separately here.
ast_node_t* parse_term(parser_t* p) {
    int negate = (parser_current(p).token == TOKEN_MINUS);
    token_t neg_tok;

    if (negate) {
        neg_tok = parser_advance(p);
    }

    ast_node_t* left =
        (parser_current(p).token == TOKEN_IDENTIFIER) ? parse_call(p) : parse_atomic(p);

    if (negate) {
        left = make_node(NODE_NEG, neg_tok, left, NULL);
    }

    while (parser_current(p).token == TOKEN_PLUS || parser_current(p).token == TOKEN_MINUS) {
        token_t op = parser_advance(p);
        ast_node_t* right =
            (parser_current(p).token == TOKEN_IDENTIFIER) ? parse_call(p) : parse_atomic(p);
        left = make_node(NODE_BINOP, op, left, right);
    }

    return left;
}

// call = IDENTIFIER { atomic }
// A bare identifier (zero trailing atomics) is just a variable reference;
// one or more trailing atomics form a left-associative application chain.
ast_node_t* parse_call(parser_t* p) {
    token_t tok = parser_match(p, TOKEN_IDENTIFIER);
    ast_node_t* left = make_node(NODE_VAR, tok, NULL, NULL);

    while (is_atomic_start(parser_current(p).token)) {
        ast_node_t* arg = parse_atomic(p);
        left = make_node(NODE_APPLY, tok, left, arg);
    }

    return left;
}

// atomic = INT | STR | IDENTIFIER | OPENPAREN exprStatement CLOSEPAREN
// Parses an atomic token
ast_node_t* parse_atomic(parser_t* p) {
    token_t tok = parser_current(p);

    switch (tok.token) {
    case TOKEN_INT:
        parser_advance(p);
        return make_node(NODE_INT, tok, NULL, NULL);
    case TOKEN_STR:
        parser_advance(p);
        return make_node(NODE_STR, tok, NULL, NULL);
    case TOKEN_IDENTIFIER:
        parser_advance(p);
        return make_node(NODE_VAR, tok, NULL, NULL);
    case TOKEN_OPENPAREN: {
        parser_advance(p);
        ast_node_t* inner = parse_expr_statement(p);
        parser_match(p, TOKEN_CLOSEPAREN);
        return inner;
    }
    // TODO: Change to printk/LOGERR for Zephyr
    default:
        fprintf(stderr, "parse error: unexpected token %d\n", tok.token);
        exit(1);
    }
}
