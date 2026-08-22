#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <stddef.h>
#include <stdint.h>

/*No individual free, you either create a checkpoint that
you  can rewind to for temporary values or you clear the whole thing*/

typedef struct {
    uint8_t* base; // start of the memory block
    size_t size;   // size of the memory arena
    size_t offset; // current position in memory arena (bytes used)
} Memrina;

/*checkpoint which you can rewind to */

typedef struct {
    size_t offset;
} Memrina_Checkpoint;

/* Default alignment used by arena_alloc(). Override with arena_alloc_aligned()
for special cases.  */
#define MEMRINA_DEFAULT_ALIGNMENT (sizeof(void*))

// allocate memory and create memory arena
Memrina* memrina_create(size_t size);

// free allocated memory from arena
void memrina_destroy(Memrina* arena);

// initialise memory arena
void memrina_init(Memrina* arena, uint8_t* buffer, size_t size);

// allocates nbytes aligned to alignment (must be a power of two)
void* memrina_alloc_aligned(Memrina* arena, size_t nbytes, size_t alignment);

// allocates nbytes aligned to the default alignment. Returns NULL on
// out-of-memory
void* memrina_alloc(Memrina* arena, size_t nbytes);

// zero-initialized allocation like calloc
void* memrina_calloc(Memrina* arena, size_t nbytes);

// allocates count items of size bytes each, returns NULL if the multiply would overflow
void* memrina_alloc_array(Memrina* arena, size_t count, size_t size);

// copies nbytes from src into the arena
void* memrina_memdup(Memrina* arena, const void* src, size_t nbytes);

// copies len bytes of src into the arena and null terminates it
char* memrina_strndup(Memrina* arena, const char* src, size_t len);

// take a checkpoint of the current memory block
Memrina_Checkpoint memrina_set_check(Memrina* arena);

/* restore arena to previous checkpoint, freeing everything allocated since.
Must be called with checkpoint from same arena */
void memrina_restore_check(Memrina* arena, Memrina_Checkpoint checkpoint);

// free everything at once, does not zero the memory.
void memrina_clear(Memrina* arena);

// returns bytes currently in use
size_t memrina_usage(Memrina* arena);

// returns bytes currently free
size_t memrina_remaining(Memrina* arena);

#endif
