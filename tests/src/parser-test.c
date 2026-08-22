#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static const char* node_type_str(node_type_t type)
{
    switch (type) {
        case NODE_INT:      return "INT";
        case NODE_STR:      return "STR";
        case NODE_LIST:     return "LIST";
        case NODE_VAR:      return "VAR";
        case NODE_BINOP:    return "BINOP";
        case NODE_ASSIGN:   return "ASSIGN";
        case NODE_IF:       return "IF";
        case NODE_FN:       return "FN";
        case NODE_LAMBDA:   return "LAMBDA";
        case NODE_APPLY:    return "APPLY";
        case NODE_RETURN:   return "RETURN";
        case NODE_TAILCALL: return "TAILCALL";
        case NODE_DOLLAR:   return "DOLLAR";
        case NODE_BLOCK:    return "BLOCK";
        default:            return "???";
    }
}

static void print_ast(ast_node_t* node, int depth)
{
    if (!node) return;

    for (int i = 0; i < depth; i++)
        printf("  ");

    printf("%s", node_type_str(node->type));

    if (node->token.str && node->token.len > 0)
        printf(" \"%.*s\"", node->token.len, node->token.str);

    printf("\n");

    if (node->cond) {
        for (int i = 0; i < depth + 1; i++)
            printf("  ");
        printf("cond:\n");
        print_ast(node->cond, depth + 2);
    }

    print_ast(node->left, depth + 1);
    print_ast(node->right, depth + 1);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    char buf[1024];

    printf("enter multi-line program, type EOF on its own line to parse:\n");

    char input[4096] = {0};
    while (fgets(buf, sizeof(buf), stdin)) {
        if (strcmp(buf, "EOF\n") == 0 || strcmp(buf, "EOF") == 0)
            break;
        strncat(input, buf, sizeof(input) - strlen(input) - 1);
    }

    char* ptr = input;
    token_t* tokens = get_token_list(&ptr);

    if (!tokens) {
        fprintf(stderr, "tokenise failed\n");
        return 1;
    }

    printf("\n--- tokens ---\n");
    int i = 0;
    while (tokens[i].token != TOKEN_EOF) {
        printf("Token: %d | Length: %d", tokens[i].token, tokens[i].len);
        if (tokens[i].str && tokens[i].len > 0)
            printf(" | \"%.*s\"", tokens[i].len, tokens[i].str);
        printf("\n");
        i++;
    }

    printf("\n--- AST ---\n");
    parser_t p = parser_init(tokens);
    ast_node_t* ast = parse_program(&p);
    print_ast(ast, 0);

    free(tokens);
    return 0;
}
