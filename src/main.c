#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: mu-lambda <file.mu>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    value_t* result = run_interpreter(source);

    if (!result) {
        fprintf(stderr, "evaluation failed\n");
        free(source);
        return 1;
    }

    switch (result->valueType) {
        case VAR_INT:
            printf("%d\n", result->value.integer);
            break;
        case VAR_STRING:
            printf("%s\n", result->value.string);
            break;
        default:
            break;
    }

    value_release(result);
    free(source);
    return 0;
}
