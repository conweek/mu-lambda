#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tokeniser.h"

token_t tokenise(char** str)
{

    while (isspace(**str))
        (*str)++;

    token_t token = {
        .token = TOKEN_ERR,
        .str = NULL,
        .len = 0
    };

    // Handle single char tokens
    switch (**str) {
        case '\0':
            UPDATE_TOKEN(TOKEN_EOF, *str, 0);
            return token;
        case '\n':
            UPDATE_TOKEN(TOKEN_EOF, *str, 0);
            return token;
        case '+':
            UPDATE_TOKEN(TOKEN_PLUS, *str, 1);
            (*str)++;
            return token;
        case '-':
            if (*(*str + 1) == '>') {
                UPDATE_TOKEN(TOKEN_ARROW, *str, 2);
                (*str) += 2;
            } else {
                UPDATE_TOKEN(TOKEN_MINUS, *str, 1);
                (*str)++;
            }
            return token;
        case '>':
            UPDATE_TOKEN(TOKEN_GREATERTHAN, *str, 1);
            (*str)++;
            return token;
        case '<':
            UPDATE_TOKEN(TOKEN_LESSTHAN, *str, 1);
            (*str)++;
            return token;
        case '(':
            UPDATE_TOKEN(TOKEN_OPENPAREN, *str, 1);
            (*str)++;
            return token;
        case ')':
            UPDATE_TOKEN(TOKEN_CLOSEPAREN, *str, 1);
            (*str)++;
            return token;
        case '$':
            UPDATE_TOKEN(TOKEN_DOLLARSIGN, *str, 1);
            (*str)++;
            return token;
        case '\\':
            UPDATE_TOKEN(TOKEN_LAMBDA, *str, 1);
            (*str)++;
            return token;
        case ':':
            UPDATE_TOKEN(TOKEN_COLON, *str, 1);
            (*str)++;
            return token;
        default:
            break;
    }

    // Handle comments
    if (**str == '/' && *(*str + 1) == '/') {
        token.token = TOKEN_COMMENT;
        token.str = *str;
        (*str) += 2;
        while (**str != '\0' && **str != '\n') {
            token.len++;
            (*str)++;
        }
        token.len += 2;
        return token;
    }

    // Handle integers
    if (isdigit(**str)) {
        token.token = TOKEN_INT;
        token.str = *str;
        while (isdigit(**str)) {
            token.len++;
            (*str)++;
        }
        return token;
    }

    // Handle strings
    if (**str == '"') {
        (*str)++;
        token.token = TOKEN_STR;
        token.str = *str;
        while (**str != '\0' && **str != '"') {
            token.len++;
            (*str)++;
        }
        if (**str == '\0') {
            UPDATE_TOKEN(TOKEN_ERR, NULL, 0);
            return token;
        }
        (*str)++;
        return token;
    }

    // Handle lists
    if (**str == '[') {
        (*str)++;
        token.token = TOKEN_LIST;
        token.str = *str;
        while (**str != '\0' && **str != ']') {
            token.len++;
            (*str)++;
        }
        if (**str == '\0') {
            UPDATE_TOKEN(TOKEN_ERR, NULL, 0);
            return token;
        }
        (*str)++;
        return token;
    }

    // Handle identifiers and keywords
    if (isalpha(**str) || **str == '_') {
        token.token = TOKEN_IDENTIFIER;
        token.str = *str;
        while (isalnum(**str) || **str == '_') {
            token.len++;
            (*str)++;
        }
        if (token.len == 2 && strncmp(token.str, "if", 2) == 0)
            token.token = TOKEN_IF;
        else if (token.len == 4 && strncmp(token.str, "else", 4) == 0)
            token.token = TOKEN_ELSE;
        else if (token.len == 2 && strncmp(token.str, "fn", 2) == 0)
            token.token = TOKEN_FUNCTION;
        else if (token.len == 2 && strncmp(token.str, "ts", 2) == 0)
            token.token = TOKEN_TAILCALL;
        return token;
    }

    // Handle not equals operator
    if (**str == '!') {
        (*str)++;
        if (**str == '=') {
            UPDATE_TOKEN(TOKEN_NOTEQUALTO, *str - 1, 2);
            (*str)++;
        } else {
            UPDATE_TOKEN(TOKEN_ERR, NULL, 0);
        }
        return token;
    }

    // Handle assignment and equals operators
    if (**str == '=') {
        (*str)++;
        if (**str == '=') {
            UPDATE_TOKEN(TOKEN_EQUALTO, *str - 1, 2);
            (*str)++;
        } else {
            UPDATE_TOKEN(TOKEN_ASSIGNMENT, *str - 1, 1);
        }
        return token;
    }

    return token;
}

token_t* get_token_list(char** str)
{
    // Worst case, str is entirely packed with valid tokens
    // (to be replaced with memory arena)
    token_t* tokenList = (token_t*)malloc(sizeof(token_t) * strlen(*str));

    // Check malloc succeeded 
    if (!tokenList)
        return NULL;

    char* tempStr = *str;
    int count = 0;

    do {
        tokenList[count] = tokenise(&tempStr);
    } while (tokenList[count++].token != TOKEN_EOF);

    return tokenList;
}
