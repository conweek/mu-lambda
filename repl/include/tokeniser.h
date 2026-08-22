#ifndef TOKENISER_H_
#define TOKENISER_H_

#define UPDATE_TOKEN(type, s, size)                                                                \
    do {                                                                                           \
        token.token = type;                                                                        \
        token.str = s;                                                                             \
        token.len = size;                                                                          \
    } while (0)

typedef enum atomic_token_t {
    // File stuff
    TOKEN_ERR,
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_UNUSED,
    // Types
    TOKEN_INT,
    TOKEN_STR,
    TOKEN_IDENTIFIER,
    // BitWise 
    TOKEN_COMPLIMENT,
    TOKEN_OR,
    TOKEN_AND,
    TOKEN_XOR,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,
    // Arithmitic
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_TIMES,
    TOKEN_DIVIDE,
    TOKEN_MODULO,
    // Comparision
    TOKEN_GREATERTHAN,
    TOKEN_GREATERTHANEQUAL,
    TOKEN_LESSTHAN,
    TOKEN_LESSTHANEQUAL,
    TOKEN_EQUALTO,
    TOKEN_NOTEQUALTO,
    // Keywords
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_ASSIGNMENT,
    TOKEN_LAMBDA,
    TOKEN_FUNCTION,
    TOKEN_TAILCALL,
    TOKEN_RETURN,
    TOKEN_ARROW,
    TOKEN_COLON,
    TOKEN_COMMENT,
    TOKEN_OPENPAREN,
    TOKEN_CLOSEPAREN,
    TOKEN_END,
    TOKEN_ENTRYPOINT
} atomic_token_t;

typedef struct Token {
    atomic_token_t token;
    int len;
    char* str;
} token_t;

token_t tokenise(char** str);

token_t* get_token_list(char** str);

#endif
