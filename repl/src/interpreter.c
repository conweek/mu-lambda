#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "tokeniser.h"
#include "parser.h"
#include "interpreter.h"
#include "builtins.h"
#include "mu_arena.h"
#include "mu_console.h"

/* The arena has no individual free, so a long running program can fill it. The
 * makers below all fail by returning NULL, which unwinds silently, so say it
 * once per submission rather than leaving the program to stop mid output. */
static bool oom_reported;

/* Nothing checked ctrl c while a program ran, so an endless loop could only be
 * escaped by resetting the board. Latched because the console clears the flag
 * as it is read. */
static bool interrupted;

/* A program that asks to stop, via the halt builtin. Unwinds exactly like an
 * interrupt so the caller rolls the whole submission back, which is what a
 * game over wants: say its piece, then leave nothing behind. */
static bool halted;

void mu_halt(void) {
    halted = true;
}

/* Wiping the arena mid evaluation would take the running program's own AST
 * and scope with it, so the builtin only raises a flag and stops. The caller
 * does the clearing once evaluation has unwound. */
static bool reset_requested;

void mu_request_reset(void) {
    reset_requested = true;
    halted = true;
}

bool mu_reset_requested(void) {
    return reset_requested;
}

static bool check_interrupt(void) {
    if (interrupted) {
        return true;
    }

    if (mu_interrupted()) {
        printk("[!] Interrupted\n");
        interrupted = true;
    }

    return interrupted;
}

static void* session_alloc(size_t nbytes) {
    void* ptr = memrina_alloc(mu_session, nbytes);

    if (!ptr && !oom_reported) {
        printk("[!] Error: session arena exhausted, %zu bytes free\n",
               memrina_remaining(mu_session));
        oom_reported = true;
    }

    return ptr;
}

value_t* make_int(int val) {
    value_t* value = (value_t*)session_alloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_INT;
    value->value.integer = val;
    value->is_return = 0;
    value->refcount = 1;

    return value;
}

value_t* make_no_result() {
    value_t* value = (value_t*)session_alloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_NORESULT;
    value->is_return = 0;
    value->refcount = 1;

    return value;
}

value_t* make_error() {
    value_t* value = (value_t*)session_alloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_ERROR;
    value->is_return = 0;
    value->refcount = 1;

    return value;
}

static inline int is_int(value_t* node) {
    return (node->valueType == VAR_INT);
}

static inline int is_str(value_t* node) {
    return (node->valueType == VAR_STRING);
}

value_t* convert_value(node_type_t type, char* val, int len) {
    value_t* value = (value_t*)session_alloc(sizeof(value_t));

    if (!value) {
        return NULL;
    }

    value->valueType = VAR_UNKNOWN;
    value->is_return = 0;
    value->refcount = 1;

    switch (type) {
    case NODE_INT: {
        char buf[32];
        int n = len < 31 ? len : 31;
        memcpy(buf, val, n);
        buf[n] = '\0';
        value->valueType = VAR_INT;
        value->value.integer = strtol(buf, NULL, 0);
        break;
    }
    case NODE_STR: {
        // Decoding only ever shrinks, so the raw span is a safe size
        char* s = session_alloc(len + 1);

        if (!s) {
            return NULL;
        }

        /* The reader drops anything outside printable ASCII, so control
         * characters can only reach a program written as an escape pair. */
        int out = 0;
        for (int i = 0; i < len; i++) {
            if (val[i] != '\\' || i + 1 >= len) {
                s[out++] = val[i];
                continue;
            }

            i++;
            switch (val[i]) {
            case 'n':
                s[out++] = '\n';
                break;
            case 't':
                s[out++] = '\t';
                break;
            case 'r':
                s[out++] = '\r';
                break;
            case 'e':
                s[out++] = 0x1b;
                break;
            default:
                // Covers \\ and \", and leaves anything unknown as written
                s[out++] = val[i];
                break;
            }
        }

        s[out] = '\0';
        value->valueType = VAR_STRING;
        value->value.string = s;
        break;
    }
    default:
        return NULL;
    }

    return value;
}

env_t* create_env(env_t* parent) {
    env_t* env = (env_t*)session_alloc(sizeof(env_t));

    if (!env) {
        return NULL;
    }

    env->parent = parent;
    env->bindings = NULL;
    env->count = 0;
    env->refcount = 1;

    return env;
}

int create_binding(env_t* env, char* name, int nameLen, value_t* value) {
    binding_t* binding = session_alloc(sizeof(binding_t));

    if (!binding) {
        return MU_BINDING_ERR;
    }

    binding->name = session_alloc(nameLen + 1);
    if (!binding->name) {
        return MU_BINDING_ERR;
    }
    memcpy(binding->name, name, nameLen);
    binding->name[nameLen] = '\0';

    binding->value = value;
    binding->next = NULL;

    if (env->bindings == NULL) {
        env->bindings = binding;
    } else {
        binding_t* curr = env->bindings;

        while (curr->next != NULL) {
            curr = curr->next;
        }

        curr->next = binding;
    }

    env->count++;
    return MU_SUCCESS;
}

/* Drop every binding made past `count`. Bindings are appended in order, so
 * this pairs with an arena rewind to undo a whole submission: the scope stops
 * pointing at values the rewind is about to take back. */
void env_truncate(env_t* env, int count) {
    if (!env || env->count <= count) {
        return;
    }

    if (count <= 0) {
        env->bindings = NULL;
        env->count = 0;
        return;
    }

    binding_t* curr = env->bindings;
    for (int i = 1; i < count && curr != NULL; i++) {
        curr = curr->next;
    }

    if (curr != NULL) {
        curr->next = NULL;
    }

    env->count = count;
}

value_t* env_lookup(env_t* env, char* name, int nameLen) {
    binding_t* binding = env->bindings;

    if (env->bindings == NULL) {
        if (env->parent == NULL) {
            return NULL;
        }

        return env_lookup(env->parent, name, nameLen);
    }

    while (binding != NULL) {
        if (strncmp(binding->name, name, nameLen) == 0 && binding->name[nameLen] == '\0') {
            return binding->value;
        }

        binding = binding->next;
    }

    if (env->parent != NULL) {
        return env_lookup(env->parent, name, nameLen);
    }

    return NULL;
}

/* Only a return, or an if that might hold one, sits in tail position. Handing
 * in_tailcall to every statement in a block turns the first bare call into a
 * thunk, and since thunks are flagged is_return the block unwinds on its first
 * line instead of running the rest of the body. */
static inline int stmt_tailcall(ast_node_t* stmt, int in_tailcall) {
    if (!in_tailcall || !stmt) {
        return 0;
    }

    return (stmt->type == NODE_RETURN || stmt->type == NODE_IF);
}

/* Statements that leave a binding behind in the current scope. Their
 * allocations outlive them, everything else is reached only for its side
 * effects and can be handed back to the arena as soon as it returns. */
static inline int stmt_binds(ast_node_t* stmt) {
    if (!stmt) {
        return 1;
    }

    return (stmt->type == NODE_ASSIGN || stmt->type == NODE_FN ||
            stmt->type == NODE_TAILCALL || stmt->type == NODE_ENTRY);
}

#define TC_MAX_SAVED 24
#define TC_NAME_MAX  31

/* One tail call's worth of bound arguments, lifted out of the arena so the
 * pass that produced them can be rewound. */
typedef struct {
    char name[TC_NAME_MAX + 1];
    int len;
    int value;
} tc_binding_t;

static tc_binding_t tc_saved[TC_MAX_SAVED];

// Was this allocated after the checkpoint, ie. would a rewind take it away
static int above_check(const void* ptr, Memrina_Checkpoint cp) {
    const uint8_t* p = (const uint8_t*)ptr;

    if (!p) {
        return 0;
    }

    return (p >= mu_session->base + cp.offset) && (p < mu_session->base + mu_session->size);
}

/* A ts loop discards everything each pass except the function it is about to
 * call and the argument going into it. Lift those clear of the arena, rewind
 * to cp, and build them again, so the loop runs in constant memory instead of
 * growing until it starves. Returns 1 when it reclaimed, 0 when the state was
 * not liftable and the arena was left alone, -1 when the arena ran out.
 *
 * Only integers can be carried over. A closure or string still points at
 * memory the rewind would take, so those cases bail out and simply keep
 * their allocations. */
static int tc_reclaim(value_t** fnp, value_t** argp, Memrina_Checkpoint cp) {
    value_t* fn = *fnp;
    value_t* arg = *argp;

    if (!fn || !arg) {
        return 0;
    }

    int arg_is_int = (arg->valueType == VAR_INT);
    if (!arg_is_int && above_check(arg, cp)) {
        return 0;
    }
    int arg_val = arg_is_int ? arg->value.integer : 0;

    ast_node_t* params = NULL;
    ast_node_t* body = NULL;
    int tailcall = 0;
    env_t* base_env = NULL;
    int saved = 0;

    // A closure older than the checkpoint survives the rewind untouched
    if (above_check(fn, cp)) {
        if (fn->valueType != VAR_CLOSURE) {
            return 0;
        }

        params = fn->value.closure.params;
        body = fn->value.closure.body;
        tailcall = fn->value.closure.tailcall;

        // Walk out to the first scope that predates the loop, saving as we go
        env_t* scope = fn->value.closure.env;
        while (scope != NULL && above_check(scope, cp)) {
            for (binding_t* b = scope->bindings; b != NULL; b = b->next) {
                if (saved >= TC_MAX_SAVED || !b->value || b->value->valueType != VAR_INT) {
                    return 0;
                }

                size_t len = strlen(b->name);
                if (len > TC_NAME_MAX) {
                    return 0;
                }

                memcpy(tc_saved[saved].name, b->name, len + 1);
                tc_saved[saved].len = (int)len;
                tc_saved[saved].value = b->value->value.integer;
                saved++;
            }
            scope = scope->parent;
        }

        if (scope == NULL) {
            return 0;
        }

        base_env = scope;
    }

    // Past this point the old values are gone, only allocation can still fail
    memrina_restore_check(mu_session, cp);

    if (params != NULL) {
        env_t* scope = base_env;

        if (saved > 0) {
            scope = create_env(base_env);
            if (!scope) {
                return -1;
            }

            /* Saved innermost first, and create_binding appends, so lookup
             * still finds the same binding it would have before. */
            for (int i = 0; i < saved; i++) {
                value_t* val = make_int(tc_saved[i].value);

                if (!val ||
                    create_binding(scope, tc_saved[i].name, tc_saved[i].len, val) != MU_SUCCESS) {
                    return -1;
                }
            }
        }

        value_t* rebuilt = session_alloc(sizeof(value_t));
        if (!rebuilt) {
            return -1;
        }

        rebuilt->valueType = VAR_CLOSURE;
        rebuilt->is_return = 0;
        rebuilt->refcount = 1;
        rebuilt->value.closure.params = params;
        rebuilt->value.closure.body = body;
        rebuilt->value.closure.env = scope;
        rebuilt->value.closure.tailcall = tailcall;
        *fnp = rebuilt;
    }

    if (arg_is_int) {
        value_t* rebuilt = make_int(arg_val);
        if (!rebuilt) {
            return -1;
        }
        *argp = rebuilt;
    }

    return 1;
}

static value_t* make_thunk(value_t* fn, value_t* arg) {
    value_t* thunk = (value_t*)session_alloc(sizeof(value_t));

    if (!thunk) {
        return NULL;
    }

    thunk->valueType = VAR_THUNK;
    thunk->is_return = 1;
    thunk->refcount = 1;
    thunk->value.thunk.fn = fn;
    thunk->value.thunk.arg = arg;

    return thunk;
}

value_t* run_interpreter(char* source, env_t* env) {
    char* ptr = source;

    oom_reported = false;
    interrupted = false;
    halted = false;
    reset_requested = false;

    token_t* tokens = get_token_list(&ptr);
    if (!tokens) {
        printk("[!] Error: failed to allocate token list\n");
        return NULL;
    }

    parser_t p = parser_init(tokens);
    ast_node_t* ast = parse_program(&p);

    if (p.error) {
        ast_free(ast);
        return NULL;
    }

    value_t* result = evaluate(ast, env);

    ast_free(ast);

    if (!result) {
        // An interrupt or a deliberate halt already said its piece
        if (!interrupted && !halted) {
            printk("[!] Error: evaluation failed\n");
        }

        return NULL;
    }

    return result;
}

value_t* evaluate(ast_node_t* node, env_t* env) {
    return evaluate_tc(node, env, 0);
}

value_t* evaluate_tc(ast_node_t* node, env_t* env, int in_tailcall) {
    if (!node) {
        return NULL;
    }

    /* Once the arena is gone nothing further can be built, unwind instead of
     * failing one allocation at a time all the way down the program. A ctrl c
     * unwinds the same way. */
    if (oom_reported || halted || check_interrupt()) {
        return NULL;
    }

    switch (node->type) {
    case NODE_ERROR:
        return NULL;
    case NODE_INT:
        return convert_value(NODE_INT, node->token.str, node->token.len);
    case NODE_STR:
        return convert_value(NODE_STR, node->token.str, node->token.len);

        SCOPED_CASE(NODE_VAR)
        value_t* val = env_lookup(env, node->token.str, node->token.len);
        if (!val) {
            printk("[!] Error: undefined variable '%.*s'\n", node->token.len, node->token.str);
            return NULL;
        }
        return val;
        END_SCOPE

        SCOPED_CASE(NODE_BINOP)
        value_t* left = evaluate_tc(node->left, env, 0);
        value_t* right = evaluate_tc(node->right, env, 0);

        if (!left || !right) {
            return NULL;
        }

        if (left->valueType != VAR_INT || right->valueType != VAR_INT) {
            printk("[!] Error: binary operator requires integer operands\n");
            return NULL;
        }

        int l = left->value.integer;
        int r = right->value.integer;

        switch (node->token.token) {
        case TOKEN_XOR:
            return make_int(l ^ r);
        case TOKEN_OR:
            return make_int(l | r);
        case TOKEN_AND:
            return make_int(l & r);
        case TOKEN_LSHIFT:
            return make_int(l << r);
        case TOKEN_RSHIFT:
            return make_int(l >> r);
        case TOKEN_PLUS:
            return make_int(l + r);
        case TOKEN_MINUS:
            return make_int(l - r);
        case TOKEN_EQUALTO:
            return make_int(l == r);
        case TOKEN_NOTEQUALTO:
            return make_int(l != r);
        case TOKEN_GREATERTHAN:
            return make_int(l > r);
        case TOKEN_GREATERTHANEQUAL:
            return make_int(l >= r);
        case TOKEN_LESSTHAN:
            return make_int(l < r);
        case TOKEN_LESSTHANEQUAL:
            return make_int(l <= r);
        case TOKEN_TIMES:
            return make_int(l * r);
        case TOKEN_MODULO:
            if (r == 0) {
                printk("[!] Error: modulo by zero\n");
                return NULL;
            }
            return make_int(l % r);
        case TOKEN_DIVIDE:
            if (r == 0) {
                printk("[!] Error: division by zero\n");
                return NULL;
            }
            return make_int(l / r);
        default:
            return NULL;
        }
        END_SCOPE

        SCOPED_CASE(NODE_COMP)
        value_t* left = evaluate_tc(node->left, env, 0);

        if (!left) {
            return NULL;
        }

        if (left->valueType != VAR_INT) {
            printk("[!] Error: compliment requires integer operand\n");
            return NULL;
        }

        left->value.integer = ~(left->value.integer);
        return left;
        END_SCOPE

        SCOPED_CASE(NODE_NEG)
        value_t* left = evaluate_tc(node->left, env, 0);

        if (!left) {
            return NULL;
        }

        if (left->valueType != VAR_INT) {
            printk("[!] Error: negation requires integer operand\n");
            return NULL;
        }

        left->value.integer = -(left->value.integer);
        return left;
        END_SCOPE

        SCOPED_CASE(NODE_ASSIGN)
        if (env_lookup(env, node->token.str, node->token.len) != NULL) {
            printk("[!] Error: variable '%.*s' already defined\n", node->token.len,
                   node->token.str);
            return NULL;
        }

        /* Working out the right hand side can burn a lot of arena to arrive at
         * one number. Keep the number, hand the working back. */
        Memrina_Checkpoint cp = memrina_set_check(mu_session);

        value_t* left = evaluate_tc(node->left, env, 0);
        if (!left) {
            return NULL;
        }

        // Only a scalar can be lifted clear, anything else still points into it
        if (left->valueType == VAR_INT) {
            int val = left->value.integer;
            memrina_restore_check(mu_session, cp);
            left = make_int(val);

            if (!left) {
                return NULL;
            }
        }

        if (create_binding(env, node->token.str, node->token.len, left) != MU_SUCCESS) {
            return NULL;
        }

        return left;
        END_SCOPE

        SCOPED_CASE(NODE_IF)
        value_t* condition = evaluate_tc(node->cond, env, 0);

        if (!condition) {
            return NULL;
        }

        if (is_int(condition) && condition->value.integer != 0) {
            env_t* child = create_env(env);
            value_t* result = evaluate_tc(node->left, child, in_tailcall);
            return result;
        } else if (node->right != NULL) {
            env_t* child = create_env(env);
            value_t* result = evaluate_tc(node->right, child, in_tailcall);
            return result;
        }

        return make_no_result();
        END_SCOPE

        SCOPED_CASE(NODE_FN)
        value_t* func = (value_t*)session_alloc(sizeof(value_t));

        if (!func) {
            return NULL;
        }

        func->valueType = VAR_CLOSURE;
        func->is_return = 0;
        func->refcount = 1;
        func->value.closure.env = env;
        func->value.closure.tailcall = 0;
        func->value.closure.body = node->right;
        func->value.closure.params = node->left;
        create_binding(env, node->token.str, node->token.len, func);
        return make_no_result();
        END_SCOPE

        SCOPED_CASE(NODE_LAMBDA)
        value_t* fn = session_alloc(sizeof(value_t));

        if (!fn) {
            return NULL;
        }

        fn->valueType = VAR_CLOSURE;
        fn->is_return = 0;
        fn->refcount = 1;
        fn->value.closure.params = node->left;
        fn->value.closure.body = node->right;
        fn->value.closure.env = env;
        fn->value.closure.tailcall = 0;
        return fn;
        END_SCOPE

        SCOPED_CASE(NODE_APPLY)
        value_t* fn = evaluate_tc(node->left, env, 0);
        value_t* arg = evaluate_tc(node->right, env, 0);

        if (!fn || !arg) {
            return NULL;
        }

        if (fn->valueType == VAR_BUILTIN) {
            value_t* result = fn->value.builtin(arg);
            if (result && result->valueType == VAR_ERROR) {
                return NULL;
            }
            return result;
        }

        if (fn->valueType == VAR_NATIVE_CLOSURE) {
            value_t* result = fn->value.native.fn(fn->value.native.ctx, arg);
            if (result && result->valueType == VAR_ERROR) {
                return NULL;
            }
            return result;
        }

        if (fn->valueType != VAR_CLOSURE) {
            printk("[!] Error: attempt to call a non-function value\n");
            return NULL;
        }

        if (in_tailcall) {
            value_t* thunk = make_thunk(fn, arg);
            return thunk;
        }

        /* fn and arg were built before this point, so a rewind to here keeps
         * them and drops only what the loop below allocates. */
        Memrina_Checkpoint cp = memrina_set_check(mu_session);

        for (;;) {
            closure_t* cl = &fn->value.closure;
            env_t* call_env = create_env(cl->env);

            ast_node_t* param = cl->params;
            create_binding(call_env, param->token.str, param->token.len, arg);

            if (param->right != NULL) {
                value_t* partial = session_alloc(sizeof(value_t));

                if (!partial) {
                    return NULL;
                }

                partial->valueType = VAR_CLOSURE;
                partial->is_return = 0;
                partial->refcount = 1;
                partial->value.closure.params = param->right;
                partial->value.closure.body = cl->body;
                partial->value.closure.env = call_env;
                partial->value.closure.tailcall = cl->tailcall;
                return partial;
            }

            value_t* result = evaluate_tc(cl->body, call_env, cl->tailcall);

            if (result) {
                result->is_return = 0;
            }


            if (result && result->valueType == VAR_THUNK) {
                fn = result->value.thunk.fn;
                arg = result->value.thunk.arg;

                if (tc_reclaim(&fn, &arg, cp) < 0) {
                    return NULL;
                }

                continue;
            }

            return result;
        }
        END_SCOPE

        SCOPED_CASE(NODE_RETURN)
        value_t* value = evaluate_tc(node->left, env, in_tailcall);

        if (!value) {
            return NULL;
        }

        value->is_return = 1;
        return value;
        END_SCOPE

        SCOPED_CASE(NODE_TAILCALL)
        value_t* func = (value_t*)session_alloc(sizeof(value_t));

        if (!func) {
            return NULL;
        }

        func->valueType = VAR_CLOSURE;
        func->is_return = 0;
        func->refcount = 1;
        func->value.closure.env = env;
        func->value.closure.tailcall = 1;
        func->value.closure.body = node->right;
        func->value.closure.params = node->left;
        create_binding(env, node->token.str, node->token.len, func);
        return make_no_result();
        END_SCOPE

        SCOPED_CASE(NODE_BLOCK)
        value_t* result = NULL;
        ast_node_t* curr = node;

        while (curr != NULL) {
            if (curr->type != NODE_BLOCK) {
                result = evaluate_tc(curr, env, stmt_tailcall(curr, in_tailcall));
                break;
            }

            ast_node_t* stmt = curr->left;

            /* Nothing this statement allocates survives it unless it binds, so
             * rewind the arena once it has run. Without this a body like the
             * chain of ifs in a lookup table costs arena on every branch it
             * tests and a long running program starves. */
            int reclaim = curr->right != NULL && !stmt_binds(stmt);
            Memrina_Checkpoint cp = memrina_set_check(mu_session);

            result = evaluate_tc(stmt, env, stmt_tailcall(stmt, in_tailcall));

            if (!result) {
                return NULL;
            }

            if (result->is_return) {
                return result;
            }

            if (reclaim) {
                memrina_restore_check(mu_session, cp);
                result = make_no_result();

                if (!result) {
                    return NULL;
                }
            }

            curr = curr->right;
        }

        return result;
        END_SCOPE

        SCOPED_CASE(NODE_ENTRY)
        evaluate_tc(node->left, env, 0);

        value_t* fn = env_lookup(env, node->left->token.str, node->left->token.len);
        if (!fn || fn->valueType != VAR_CLOSURE) {
            return NULL;
        }

        closure_t* cl = &fn->value.closure;

        if (cl->params != NULL) {
            printk("[!] Error: entry point function must take no arguments\n");
            return NULL;
        }

        /* An entry point that returns itself runs again, and nothing it built
         * carries over to the next pass, so each one starts from here. */
        Memrina_Checkpoint cp = memrina_set_check(mu_session);

        for (;;) {
            env_t* call_env = create_env(cl->env);
            value_t* result = evaluate_tc(cl->body, call_env, cl->tailcall);
            if (result) {
                result->is_return = 0;
            }

            if (result == fn) {
                memrina_restore_check(mu_session, cp);
                continue;
            }

            return result;
        }
        END_SCOPE
    }

    return NULL;
}
