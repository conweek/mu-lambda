#include "mu_arena.h"

#define MU_SESSION_SIZE 65536
// One token per character is the worst case, and STMT_MAX caps the input
#define MU_SCRATCH_SIZE 69632

static uint8_t session_buf[MU_SESSION_SIZE];
static uint8_t scratch_buf[MU_SCRATCH_SIZE];

static Memrina session;
static Memrina scratch;

Memrina* mu_session = &session;
Memrina* mu_scratch = &scratch;

void mu_arena_init(void) {
    memrina_init(&session, session_buf, sizeof(session_buf));
    memrina_init(&scratch, scratch_buf, sizeof(scratch_buf));
}
