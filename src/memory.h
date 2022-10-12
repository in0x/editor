#pragma once
#include "core.h"

struct Arena
{
    void* buffer = nullptr;
    u64 capacity = 0;
    u64 bytes_allocated = 0;
};

Arena arena_allocate(u64 capacity);
void arena_free(Arena* arena);

struct Mark // TODO(): Should this be called MemoryMark?
{
    u64 position = 0;
};

Mark arena_mark(Arena* arena);
void arena_clear_to_mark(Arena* arena, Mark mark);

void* arena_push(Arena* arena, u64 num_bytes);
void* arena_push_a(Arena* arena, u64 num_bytes, u64 alignment);

template <typename T>
T* arena_push_t(Arena* arena)
{
    return (T*)arena_push_a(arena, sizeof(T), alignof(T));
}

template <typename T>
struct Slice
{
    T* array = nullptr;
    u64 size = 0;

    T& operator[](u64 idx)
    {
        ASSERT(idx < size);
        return array[idx];
    }

    T* begin()
    {
        return array;
    }

    T* end()
    {
        return array + size;
    }

    bool is_valid() const
    {
        return array && size;
    }
};

template <typename T>
Slice<T> arena_push_array(Arena* arena, u64 size)
{
    void* allocation = arena_push_a(arena, sizeof(T) * size, alignof(T));
    return Slice<T>{(T*)allocation, size};
}

#define ARENA_DEFER_CLEAR(arena)                      \
    Mark CONCAT(mark_, __LINE__) = arena_mark(arena); \
    DEFER { arena_clear_to_mark(arena, CONCAT(mark_, __LINE__)); };\
    