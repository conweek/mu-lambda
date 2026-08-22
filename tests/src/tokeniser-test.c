#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokeniser.h"

int main(int argc, char** argv)
{
    char* buf = (char*)calloc(100, sizeof(char));
    char* ogBuf = buf;

    if (!buf)
        return 1;

    while (1) {
        printf("> ");

        int count = 0;
        if (fgets(buf, 100, stdin) != NULL) {

            token_t* tokens = get_token_list(&buf);

            while (tokens[count].token != TOKEN_EOF) {
                printf("Token: %d | Length: %d\r\n", tokens[count].token, tokens[count].len);
                count++;
            }

            buf = ogBuf;
            free(tokens);
        }
    }
    return 0;
}
