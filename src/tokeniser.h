#ifndef TOKENISER_H_
#define TOKENISER_H_

#define UPDATE_TOKEN(type, s, size)     \
    do {                                \
        token.token = type;             \
        token.str   = s;                \
        token.len  = size;              \
    } while (0)

typedef enum atomic_token_t {
    TOKEN_ERR,
    TOKEN_EOF,
    TOKEN_INT,
    TOKEN_STR,
    TOKEN_LIST,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_GREATERTHAN,
    TOKEN_LESSTHAN,
    TOKEN_EQUALTO,
    TOKEN_NOTEQUALTO,
    TOKEN_ASSIGNMENT,
    TOKEN_FUNCTION,
    TOKEN_TAILCALL,
    TOKEN_COMMENT,
    TOKEN_LAMBDA,
    TOKEN_OPENPAREN,
    TOKEN_CLOSEPAREN,
    TOKEN_DOLLARSIGN,
    TOKEN_COLON,
    TOKEN_ARROW,
    TOKEN_IDENTIFIER
}atomic_token_t;

typedef struct Token {
    atomic_token_t token;
    char* str;
    int len;
}token_t;

token_t tokenise(char** str);

token_t* get_token_list(char** str);

#endif 
