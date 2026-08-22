#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>
#include "interpreter.h"

static void print_value(value_t* val)
{
    if (!val) {
        printf("(nil)");
        return;
    }

    switch (val->valueType) {
        case VAR_INT:
            printf("%d", val->value.integer);
            break;
        case VAR_STRING:
            printf("\"%s\"", val->value.string);
            break;
        case VAR_CLOSURE:
            printf("<closure>");
            break;
        case VAR_NORESULT:
            printf("<noresult>");
            break;
        default:
            printf("<unknown>");
            break;
    }
}

static void run_child(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f)
        _exit(2);

    char input[4096] = {0};
    char buf[1024];
    while (fgets(buf, sizeof(buf), f))
        strncat(input, buf, sizeof(input) - strlen(input) - 1);
    fclose(f);

    char* ptr = input;
    token_t* tokens = get_token_list(&ptr);
    if (!tokens)
        _exit(1);

    parser_t p = parser_init(tokens);
    ast_node_t* ast = parse_program(&p);

    env_t* env = create_env(NULL);
    value_t* result = evaluate(ast, env);
    print_value(result);
    printf("\n");
    fflush(stdout);

    value_release(result);
    env_release(env);
    free(tokens);
    _exit(0);
}

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f)
        return NULL;

    char* buf = malloc(4096);
    buf[0] = '\0';

    char line[1024];
    while (fgets(line, sizeof(line), f))
        strncat(buf, line, 4096 - strlen(buf) - 1);

    fclose(f);

    int len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == ' '))
        buf[--len] = '\0';

    return buf;
}

static int run_case(const char* dir, const char* name, int* pass, int* fail)
{
    char mu_path[512];
    char exp_path[512];

    int namelen = strlen(name);
    char base[256];
    memcpy(base, name, namelen - 3);
    base[namelen - 3] = '\0';

    snprintf(mu_path, sizeof(mu_path), "%s/%s", dir, name);
    snprintf(exp_path, sizeof(exp_path), "%s/%s.expected", dir, base);

    char* expected = read_file(exp_path);
    if (!expected) {
        printf("  SKIP  %s (no .expected file)\n", name);
        return 0;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        free(expected);
        return -1;
    }

    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(expected);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        freopen("/dev/null", "w", stderr);
        run_child(mu_path);
    }

    close(pipefd[1]);

    char actual[4096] = {0};
    int total = 0;
    int n;
    while ((n = read(pipefd[0], actual + total, sizeof(actual) - total - 1)) > 0)
        total += n;
    actual[total] = '\0';
    close(pipefd[0]);

    int len = strlen(actual);
    while (len > 0 && (actual[len - 1] == '\n' || actual[len - 1] == ' '))
        actual[--len] = '\0';

    int status;
    waitpid(pid, &status, 0);
    int exited_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    if (!exited_ok) {
        printf("  FAIL  %s (crashed or exit %d)\n", name,
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        (*fail)++;
    } else if (strcmp(actual, expected) != 0) {
        printf("  FAIL  %s\n        expected: %s\n        got:      %s\n", name, expected, actual);
        (*fail)++;
    } else {
        printf("  PASS  %s\n", name);
        (*pass)++;
    }

    free(expected);
    return 0;
}

int main(int argc, char** argv)
{
    const char* dir = "tests/interpreter-cases";

    if (argc > 1)
        dir = argv[1];

    DIR* d = opendir(dir);
    if (!d) {
        fprintf(stderr, "cannot open directory %s\n", dir);
        return 1;
    }

    int pass = 0;
    int fail = 0;

    printf("--- interpreter test suite ---\n");

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        int len = strlen(ent->d_name);
        if (len < 4 || strcmp(ent->d_name + len - 3, ".mu") != 0)
            continue;

        run_case(dir, ent->d_name, &pass, &fail);
    }

    closedir(d);

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
