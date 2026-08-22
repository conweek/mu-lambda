#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>
#include "parser.h"

static int run_one(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return -1;
    }

    char input[4096] = {0};
    char buf[1024];
    while (fgets(buf, sizeof(buf), f))
        strncat(input, buf, sizeof(input) - strlen(input) - 1);
    fclose(f);

    char* ptr = input;
    token_t* tokens = get_token_list(&ptr);
    if (!tokens)
        exit(1);

    parser_t p = parser_init(tokens);
    parse_program(&p);

    free(tokens);
    return 0;
}

static int run_dir(const char* dir, int expect_fail, int* pass, int* fail)
{
    DIR* d = opendir(dir);
    if (!d) {
        fprintf(stderr, "cannot open directory %s\n", dir);
        return -1;
    }

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        int len = strlen(ent->d_name);
        if (len < 4 || strcmp(ent->d_name + len - 3, ".mu") != 0)
            continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        fflush(stdout);
        fflush(stderr);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            closedir(d);
            return -1;
        }

        if (pid == 0) {
            freopen("/dev/null", "w", stderr);
            run_one(path);
            _exit(0);
        }

        int status;
        waitpid(pid, &status, 0);
        int exited_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

        if (expect_fail) {
            if (!exited_ok) {
                printf("  PASS  %s\n", ent->d_name);
                (*pass)++;
            } else {
                printf("  FAIL  %s (expected parse error)\n", ent->d_name);
                (*fail)++;
            }
        } else {
            if (exited_ok) {
                printf("  PASS  %s\n", ent->d_name);
                (*pass)++;
            } else {
                printf("  FAIL  %s (expected to parse cleanly)\n", ent->d_name);
                (*fail)++;
            }
        }
    }

    closedir(d);
    return 0;
}

int main(int argc, char** argv)
{
    const char* base = "tests/parser-cases";

    if (argc > 1)
        base = argv[1];

    char valid_dir[512];
    char invalid_dir[512];
    snprintf(valid_dir, sizeof(valid_dir), "%s/valid", base);
    snprintf(invalid_dir, sizeof(invalid_dir), "%s/invalid", base);

    int pass = 0;
    int fail = 0;

    printf("--- valid cases (should parse) ---\n");
    run_dir(valid_dir, 0, &pass, &fail);

    printf("\n--- invalid cases (should reject) ---\n");
    run_dir(invalid_dir, 1, &pass, &fail);

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
