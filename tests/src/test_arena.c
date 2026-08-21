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

static void test_out_of_memory(void) {
    static uint8_t buf[16];
    Memrina a;
    memrina_init(&a, buf, sizeof buf);

    void *p1 = memrina_alloc(&a, 8);
    assert(p1 != NULL);

    void *p2 = memrina_alloc(&a, 1000); /* way too big, must fail cleanly */
    assert(p2 == NULL);
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

int main(void) {
    test_basic_alloc();
    test_alignment();
    test_out_of_memory();
    test_reset();
    test_mark_release();
    test_linked_list_demo();
    printf("\nAll tests passed.\n");
    return 0;
}