#include "memory.h"

Arena arena_allocate(u64 capacity)
{
    Arena result = {};
    result.buffer = (u8*)malloc(capacity);
    result.capacity = capacity;
    result.bytes_allocated = 0;
    return result;
}

void arena_free(Arena* arena)
{
    ASSERT(arena->buffer != nullptr);
    free(arena->buffer);
    arena->bytes_allocated = 0;
    arena->capacity = 0;
}

Mark arena_mark(Arena* arena)
{
    return Mark { arena->bytes_allocated };
}

void arena_clear_to_mark(Arena* arena, Mark mark)
{
    arena->bytes_allocated = mark.position;
}

Slice arena_push(Arena* arena, u64 num_bytes)
{
    if ((arena->bytes_allocated + num_bytes) > arena->capacity)
    {
        ASSERT_FAILED_MSG("Tried to allocate beyond arena capacity.");
        return Slice {};
    }

    arena->bytes_allocated += num_bytes;

    Slice result = {};
    result.buffer = (u8*)arena->buffer;
    result.parent = arena;
    result.size = num_bytes;
    return result;
}

void arena_pop(Arena* arena, Slice allocation)
{
    // TODO(): I dont think we need to carry the parent pointer to verify this.
    // But it might be useful for other validation, have to evaluate further.
    if (arena != allocation.parent)
    {
        ASSERT(arena == allocation.parent);
        return;
    }

    if ((arena->buffer + arena->bytes_allocated) != allocation.buffer)
    {
        ASSERT_FAILED_MSG("Tried to pop an allocation from arena that was not the most recent.");
        return;
    }

    arena->bytes_allocated -= allocation.size;
}
