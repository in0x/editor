#pragma once
#include "core.h"

struct Arena
{
    u8* buffer = nullptr;
    u64 capacity = 0;
    u64 bytes_allocated = 0;
};

Arena arena_allocate(u64 capacity);
void  arena_free(Arena* arena);

struct Mark // TODO(): Should this be called MemoryMark?
{
    u64 position = 0;
};

Mark arena_mark(Arena* arena);
void arena_clear_to_mark(Arena* arena, Mark mark);

struct Slice // TODO(): Should this be called MemorySlice?
{
    Arena* parent = nullptr;
    u8* buffer = nullptr;
    u64 size   = 0;

    u8& operator[](u64 idx)
    {
        ASSERT(idx < size);
        return buffer[idx];
    }

    bool is_valid() const
    {
        return (buffer != nullptr);
    }
};

Slice arena_push(Arena* arena, u64 num_bytes);
void arena_pop(Arena* arena, Slice allocation);