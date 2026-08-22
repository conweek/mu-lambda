#undef NDEBUG
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "memory-arena.h"

/* Toy "cons cell" to mimic what an FP language runtime would store. */
typedef struct Cons {
    int value;
    struct Cons *next;
} Cons;

static void test_basic_alloc(void) {
    static uint8_t buf[256];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    int *x = memrina_alloc(&a, sizeof(int));
    assert(x != NULL);
    *x = 42;

    int *y = memrina_alloc(&a, sizeof(int));
    assert(y != NULL);
    *y = 7;

    assert(*x == 42 && *y == 7);
    assert(x != y);
    printf("test_basic_alloc OK (used=%zu)\n", memrina_usage(&a));
}

static void test_alignment(void) {
    static uint8_t buf[256];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    /* Force an odd offset, then request an 8-byte aligned block and
     * check the returned pointer is actually 8-byte aligned. */
    (void)memrina_alloc(&a, 3); /* offset now 3 (before rounding) */
    void *p = memrina_alloc_aligned(&a, 16, 8);
    assert(p != NULL);
    assert(((uintptr_t)p % 8) == 0);
    printf("test_alignment OK (ptr=%p)\n", p);
}

static void test_alignment_misaligned_base(void) {
    static uint8_t buf[256];
    Memrina a;

    for (size_t skew = 1; skew <= 8; skew++) {
        memrina_init(&a, buf + skew, sizeof buf - skew);

        void *p8 = memrina_alloc_aligned(&a, 8, 8);
        assert(p8 != NULL);
        assert(((uintptr_t)p8 % 8) == 0);

        void *p16 = memrina_alloc_aligned(&a, 8, 16);
        assert(p16 != NULL);
        assert(((uintptr_t)p16 % 16) == 0);
    }
    printf("test_alignment_misaligned_base OK\n");
}

static void test_out_of_memory(void) {
    static uint8_t buf[16];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    void *p1 = memrina_alloc(&a, 8);
    assert(p1 != NULL);

    void *p2 = memrina_alloc(&a, 1000); /* way too big, must fail cleanly */
    assert(p2 == NULL);

    assert(memrina_alloc(&a, memrina_remaining(&a)) != NULL);
    assert(memrina_remaining(&a) == 0);
    assert(memrina_alloc(&a, 1) == NULL);
    assert(memrina_alloc(&a, (size_t)-1) == NULL);
    printf("test_out_of_memory OK (correctly returned NULL)\n");
}

static void test_reset(void) {
    static uint8_t buf[64];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    memrina_alloc(&a, 32);
    assert(memrina_usage(&a) == 32);

    memrina_clear(&a);
    assert(memrina_usage(&a) == 0);
    assert(memrina_remaining(&a) == 64);
    printf("test_reset OK\n");
}

static void test_mark_release(void) {
    static uint8_t buf[64];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    memrina_alloc(&a, 8); /* "long-lived" allocation, kept */
    Memrina_Checkpoint checkpoint = memrina_set_check(&a);

    memrina_alloc(&a, 16); /* scratch work inside a sub-expression */
    memrina_alloc(&a, 8);
    assert(memrina_usage(&a) == 32);

    memrina_restore_check(&a, checkpoint); /* discard the scratch work */
    assert(memrina_usage(&a) == 8);   /* the first allocation survives */

    printf("test_mark_release OK\n");
}

/* Demo: build a small linked list entirely in the arena, then drop
 * the whole list at once via reset — the "GC" a bump allocator gives
 * you for free, and a nice thing to show off at a hackathon demo. */
static void test_linked_list_demo(void) {
    static uint8_t buf[1024];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    Cons *head = NULL;
    for (int i = 0; i < 10; i++) {
        Cons *node = memrina_alloc(&a, sizeof(Cons));
        assert(node != NULL);
        node->value = i;
        node->next = head;
        head = node;
    }

    int sum = 0;
    for (Cons *c = head; c != NULL; c = c->next) {
        sum += c->value;
    }
    assert(sum == 45); /* 0+1+...+9 */

    printf("test_linked_list_demo OK (sum=%d, used=%zu bytes for 10 cons cells)\n",
           sum, memrina_usage(&a));

    memrina_clear(&a); /* whole list gone, O(1) */
    assert(memrina_usage(&a) == 0);
}

/* should hand back a working arena without the
 * caller supplying any backing memory. */
static void test_create_basic_alloc(void) {
    Memrina *a = memrina_create(256);
    assert(a != NULL);
 
    int *x = memrina_alloc(a, sizeof(int));
    assert(x != NULL);
    *x = 42;
 
    int *y = memrina_alloc(a, sizeof(int));
    assert(y != NULL);
    *y = 7;
 
    assert(*x == 42 && *y == 7);
    assert(x != y);
    printf("test_create_basic_alloc OK (used=%zu)\n", memrina_usage(a));
 
    memrina_destroy(a);
}
 
/* A zero-size (or otherwise failing) backing allocation should
 * still be handled gracefully rather than crashing. Zero-size malloc
 * is implementation-defined (may return NULL or a valid unique
 * pointer), so we only assert that arena_create() doesn't crash and,
 * if it does succeed, that the arena correctly reports no room. */
static void test_create_zero_size(void) {
    Memrina *a = memrina_create(0);
    if (a != NULL) {
        void *p = memrina_alloc(a, 1);
        assert(p == NULL); /* zero capacity: nothing should fit */
        memrina_destroy(a);
    }
    printf("test_create_zero_size OK\n");
}
 
/* arena_destroy() must accept NULL without crashing, same contract
 * as free(). */
static void test_destroy_null(void) {
    memrina_destroy(NULL);
    printf("test_destroy_null OK\n");
}
 
/* Sanity check that two independently-created arenas don't share
 * memory or state. */
static void test_create_multiple_independent(void) {
    Memrina *a1 = memrina_create(64);
    Memrina *a2 = memrina_create(64);
    assert(a1 != NULL && a2 != NULL);
 
    int *x = memrina_alloc(a1, sizeof(int));
    int *y = memrina_alloc(a2, sizeof(int));
    *x = 1;
    *y = 2;

    memrina_clear(a2); /* should not affect a1 */
 
    assert(*x == 1);
    assert(memrina_usage(a1) == sizeof(int));
    assert(memrina_usage(a2) == 0);
    printf("test_create_multiple_independent OK\n");
 
    memrina_destroy(a1);
    memrina_destroy(a2);
}
 
static void test_calloc_zeroes(void) {
    static uint8_t buf[64];
    Memrina a;

    memset(buf, 0xAA, sizeof buf);
    memrina_init(&a, buf, sizeof buf);

    unsigned char *z = memrina_calloc(&a, 16);
    assert(z != NULL);
    for (int i = 0; i < 16; i++) {
        assert(z[i] == 0);
    }
    printf("test_calloc_zeroes OK\n");
}

static void test_bad_alignment(void) {
    static uint8_t buf[64];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    assert(memrina_alloc_aligned(&a, 4, 0) == NULL);
    assert(memrina_alloc_aligned(&a, 4, 3) == NULL);
    assert(memrina_alloc_aligned(&a, 4, 6) == NULL);
    printf("test_bad_alignment OK\n");
}

static void test_null_arena(void) {
    assert(memrina_alloc(NULL, 8) == NULL);
    assert(memrina_alloc_aligned(NULL, 8, 8) == NULL);
    assert(memrina_calloc(NULL, 8) == NULL);
    assert(memrina_usage(NULL) == 0);
    assert(memrina_remaining(NULL) == 0);

    memrina_clear(NULL);
    memrina_restore_check(NULL, memrina_set_check(NULL));
    printf("test_null_arena OK\n");
}

static void test_usage_plus_remaining(void) {
    static uint8_t buf[64];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    assert(memrina_usage(&a) + memrina_remaining(&a) == 64);
    memrina_alloc(&a, 5);
    assert(memrina_usage(&a) + memrina_remaining(&a) == 64);
    memrina_alloc_aligned(&a, 8, 16);
    assert(memrina_usage(&a) + memrina_remaining(&a) == 64);
    printf("test_usage_plus_remaining OK\n");
}

static void test_create_no_header_overlap(void) {
    Memrina *a = memrina_create(128);
    assert(a != NULL);

    uint8_t *first = memrina_alloc(a, 1);
    assert(first != NULL);
    assert(first >= (uint8_t *)a + sizeof(Memrina));

    size_t before = memrina_usage(a);
    memset(first, 0xFF, 120);
    assert(memrina_usage(a) == before);
    assert(memrina_remaining(a) == 128 - before);

    memrina_destroy(a);
    printf("test_create_no_header_overlap OK\n");
}

static void test_alloc_array(void) {
    static uint8_t buf[256];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    int *p = memrina_alloc_array(&a, 10, sizeof(int));
    assert(p != NULL);
    for (int i = 0; i < 10; i++) {
        p[i] = i;
    }
    assert(p[9] == 9);

    assert(memrina_alloc_array(&a, (size_t)-1, 2) == NULL);
    assert(memrina_alloc_array(&a, (size_t)-1 / 4 + 1, 4) == NULL);
    assert(memrina_alloc_array(&a, 1000, sizeof(int)) == NULL);
    assert(memrina_alloc_array(&a, 0, sizeof(int)) != NULL);
    printf("test_alloc_array OK\n");
}

static void test_memdup_strndup(void) {
    static uint8_t buf[256];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    const char *src = "hello world";

    char *d = memrina_memdup(&a, src, 5);
    assert(d != NULL);
    assert(memcmp(d, "hello", 5) == 0);
    assert(d != src);

    memset(buf, 0xAA, sizeof buf);
    memrina_init(&a, buf, sizeof buf);

    size_t before = memrina_usage(&a);
    char *s = memrina_strndup(&a, src, 5);
    assert(s != NULL);
    assert(memrina_usage(&a) - before == 6);
    assert(strcmp(s, "hello") == 0);
    assert(s[5] == 0);

    char *s2 = memrina_strndup(&a, src, 5);
    assert(s2 != NULL);
    assert(s2 != s);
    assert(strcmp(s, "hello") == 0);

    assert(memrina_memdup(&a, NULL, 4) == NULL);
    assert(memrina_strndup(&a, NULL, 4) == NULL);
    assert(memrina_strndup(&a, src, 9999) == NULL);
    assert(memrina_alloc_array(NULL, 4, 4) == NULL);
    printf("test_memdup_strndup OK\n");
}

int main(void) {
    test_basic_alloc();
    test_alignment();
    test_alignment_misaligned_base();
    test_out_of_memory();
    test_reset();
    test_mark_release();
    test_linked_list_demo();
    test_create_basic_alloc();
    test_create_zero_size();
    test_destroy_null();
    test_create_multiple_independent();
    test_calloc_zeroes();
    test_bad_alignment();
    test_null_arena();
    test_usage_plus_remaining();
    test_create_no_header_overlap();
    test_alloc_array();
    test_memdup_strndup();
    printf("\nAll tests passed.\n");
    return 0;
} 