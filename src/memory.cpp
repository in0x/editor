#include "memory.h"

Arena arena_allocate(u64 capacity)
{
    Arena result = {};
    result.buffer = malloc(capacity);
    result.capacity = capacity;
    result.bytes_allocated = 0;
    memset(result.buffer, 0, result.bytes_allocated);
    return result;
}

void arena_free(Arena *arena)
{
    ASSERT(arena->buffer != nullptr);
    free(arena->buffer);
    arena->bytes_allocated = 0;
    arena->capacity = 0;
}

Mark arena_mark(Arena *arena)
{
    return Mark{arena->bytes_allocated};
}

void arena_clear_to_mark(Arena *arena, Mark mark)
{
    memset((u8 *)arena->buffer + mark.position, 0, arena->bytes_allocated - mark.position);
    arena->bytes_allocated = mark.position;
}

Slice arena_push(Arena *arena, u64 num_bytes)
{
    if ((arena->bytes_allocated + num_bytes) > arena->capacity)
    {
        ASSERT_FAILED_MSG("Tried to allocate beyond arena capacity.");
        return Slice{};
    }

    Slice result = {};
    result.buffer = (u8 *)arena->buffer + arena->bytes_allocated;
    result.parent = arena;
    result.size = num_bytes;

    arena->bytes_allocated += num_bytes;
    return result;
}

Slice arena_push_a(Arena *arena, u64 num_bytes, u64 alignment)
{
    u64 mask = alignment - 1;
    ASSERT((alignment > 0) && ((alignment & mask) == 0));

    // Check how many bytes extra we need to so we can align the address of arena top
    u64 misalignment = (uintptr_t(arena->buffer) + arena->bytes_allocated) % alignment;
    Slice allocation = arena_push(arena, num_bytes + misalignment);

    // Align the allocated pointer
    allocation.buffer += misalignment;
    return allocation;
}