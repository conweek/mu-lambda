#include "memory-arena.h"
#include <stdint.h>
#include <stdlib.h>

// Round n up to the next multiple of align. align must be a power of two
static uintptr_t align_up(uintptr_t n, size_t align) {
  return (n + (align - 1)) & ~(uintptr_t)(align - 1);
}

Memrina *memrina_create(size_t size) {
  Memrina *memrina = malloc(sizeof(Memrina) + size);

  if (memrina == NULL) {
    return NULL;
  }
  // buffer begins after the header in the contiguous block
  uint8_t *buffer = (uint8_t *)memrina + sizeof(Memrina);

  memrina_init(memrina, buffer, size);

  return memrina;
}

void memrina_destroy(Memrina *arena) {
  if (arena == NULL) {
    return;
  }
  free(arena);
}

void memrina_init(Memrina *arena, uint8_t *buffer, size_t size) {
  arena->base = buffer;
  arena->size = size;
  arena->offset = 0;
}

void *memrina_alloc_aligned(Memrina *arena, size_t nbytes, size_t alignment) {
  if (arena == NULL || arena->base == NULL) {
    return NULL;
  }

  // check alignment is a power of two
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  // Align the address
  uintptr_t base = (uintptr_t)arena->base;
  uintptr_t aligned_addr = align_up(base + arena->offset, alignment);
  size_t aligned_offset = (size_t)(aligned_addr - base);

  if (aligned_offset > arena->size || nbytes > arena->size - aligned_offset) {
    return NULL; /* out of memory */
  }

  void *ptr = arena->base + aligned_offset;
  arena->offset = aligned_offset + nbytes;
  return ptr;
}

void *memrina_alloc(Memrina *arena, size_t nbytes) {
  return memrina_alloc_aligned(arena, nbytes, MEMRINA_DEFAULT_ALIGNMENT);
}

void *memrina_calloc(Memrina *arena, size_t nbytes) {
  void *ptr = memrina_alloc(arena, nbytes);
  if (ptr != NULL) {
    uint8_t *point = (uint8_t *)ptr;
    for (size_t i = 0; i < nbytes; i++) {
      point[i] = 0;
    }
  }
  return ptr;
}

Memrina_Checkpoint memrina_set_check(Memrina *arena) {
  Memrina_Checkpoint checkpoint;
  checkpoint.offset = (arena == NULL) ? 0 : arena->offset;
  return checkpoint;
}

void memrina_restore_check(Memrina *arena, Memrina_Checkpoint checkpoint) {
  if (arena == NULL) {
    return;
  }

  if (checkpoint.offset <= arena->offset) {
    arena->offset = checkpoint.offset;
  }
}

void memrina_clear(Memrina *arena) {
  if (arena == NULL) {
    return;
  }

  arena->offset = 0;
}

size_t memrina_usage(Memrina *arena) {
  return (arena == NULL) ? 0 : arena->offset;
}

size_t memrina_remaining(Memrina *arena) {
  return (arena == NULL) ? 0 : arena->size - arena->offset;
}
