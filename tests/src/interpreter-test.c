#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"

static void print_value(value_t* val)
{
    if (!val) {
        printf("(nil)\n");
        return;
    }

    switch (val->valueType) {
        case VAR_INT:
            printf("%d\n", val->value.integer);
            break;
        case VAR_STRING:
            printf("\"%s\"\n", val->value.string);
            break;
        case VAR_CLOSURE:
            printf("<closure>\n");
            break;
        default:
            printf("<unknown>\n");
            break;
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    char buf[1024];
    env_t* global = create_env(NULL);

    printf("mu-lambda repl (type EOF on its own line to quit)\n\n");

    for (;;) {
        printf("> ");
        fflush(stdout);

        char input[4096] = {0};

        while (fgets(buf, sizeof(buf), stdin)) {
            if (strcmp(buf, "EOF\n") == 0 || strcmp(buf, "EOF") == 0)
                break;
            strncat(input, buf, sizeof(input) - strlen(input) - 1);
        }

        if (feof(stdin) || (strcmp(buf, "EOF\n") == 0 || strcmp(buf, "EOF") == 0)) {
            if (input[0] == '\0')
                break;
        }

        printf("\n--- input ---\n[%s]\n", input);
        fflush(stdout);
        printf("\n--- tokens ---\n");
        fflush(stdout);
        char* ptr = input;
        token_t* tokens = get_token_list(&ptr);

        if (!tokens) {
            fprintf(stderr, "tokenise failed\n");
            continue;
        }

        int i = 0;
        while (tokens[i].token != TOKEN_EOF) {
            printf("Token: %d | Length: %d", tokens[i].token, tokens[i].len);
            if (tokens[i].str && tokens[i].len > 0)
                printf(" | \"%.*s\"", tokens[i].len, tokens[i].str);
            printf("\n");
            i++;
        }

        printf("\n--- AST ---\n");
        fflush(stdout);
        parser_t p = parser_init(tokens);
        ast_node_t* ast = parse_program(&p);
        fflush(stdout);

        printf("\n--- result ---\n");
        fflush(stdout);
        value_t* result = evaluate(ast, global);
        print_value(result);
        printf("\n");

        free(tokens);
    }

    return 0;
}
