#ifndef MU_ARENA_H
#define MU_ARENA_H

#include "memory-arena.h"

// Lives until soft reset, holds source, AST, environments, bindings and values
extern Memrina* mu_session;

// Cleared before every submission, holds the token list and scratch work
extern Memrina* mu_scratch;

// Sets up both arenas over their static buffers, call once at startup
void mu_arena_init(void);

#endif
