#include "memory-arena.h"

// Round n up to the next multiple of align. align must be a power of two
static size_t align_up(size_t n, size_t align) {
    return (n + (align - 1)) & ~(align - 1);
}

void memrina_init(Memrina* arena, uint8_t* buffer, size_t size) {

    arena->base = buffer;
    arena->size = size;
    arena->offset = 0;

}


void* memrina_alloc_aligned(Memrina* arena, size_t nbytes, size_t alignment) {
    if (arena == NULL || arena->base == NULL) {
        return NULL;
    }

    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }
 
    size_t aligned_offset = align_up(arena->offset, alignment);
 

    if (aligned_offset > arena->size || nbytes > arena->size - aligned_offset) {
        return NULL; /* out of memory */
    }
 
    void *ptr = arena->base + aligned_offset;
    arena->offset = aligned_offset + nbytes;
    return ptr;
}

void* memrina_alloc(Memrina* arena, size_t nbytes) {
    return memrina_alloc_aligned(arena, nbytes, MEMRINA_DEFAULT_ALIGNMENT);
}

void* memrina_calloc(Memrina* arena, size_t nbytes) {
    void* ptr = memrina_alloc(arena, nbytes);
    if (ptr != NULL) {
        uint8_t* point = (uint8_t*)ptr;
        for (size_t i = 0; i < nbytes; i++) {
            point[i] = 0;
        }
    }
    return ptr;
}

Memrina_Checkpoint memrina_set_check(Memrina* arena) {
    Memrina_Checkpoint checkpoint;
    checkpoint.offset = arena->offset;
    return checkpoint;
}

void memrina_restore_check(Memrina* arena, Memrina_Checkpoint checkpoint) {
    if (checkpoint.offset <= arena->offset) {
        arena->offset = checkpoint.offset;
    }
}

void memrina_clear(Memrina* arena) {
    arena->offset = 0;
}

size_t memrina_usage(Memrina* arena) {

    return arena->offset;
}

size_t memrina_remaining(Memrina* arena) {

    return arena->size - arena->offset;   
}