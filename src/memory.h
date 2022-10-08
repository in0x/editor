#pragma once
#include "core.h"

struct Arena
{
    void *buffer = nullptr;
    u64 capacity = 0;
    u64 bytes_allocated = 0;
};

Arena arena_allocate(u64 capacity);
void arena_free(Arena *arena);

struct Mark // TODO(): Should this be called MemoryMark?
{
    u64 position = 0;
};

Mark arena_mark(Arena *arena);
void arena_clear_to_mark(Arena *arena, Mark mark);

struct Slice // TODO(): Should this be called MemorySlice?
{
    Arena *parent = nullptr;
    u8 *buffer = nullptr;
    u64 size = 0;

    u8 &operator[](u64 idx)
    {
        ASSERT(idx < size);
        return buffer[idx];
    }

    bool is_valid() const
    {
        return (buffer != nullptr);
    }
};

Slice arena_push(Arena *arena, u64 num_bytes);
Slice arena_push_a(Arena *arena, u64 num_bytes, u64 alignment);

template <typename T>
T *arena_push_t(Arena *arena)
{
    Slice allocation = arena_push_a(arena, sizeof(T), alignof(T));
    return (T *)allocation.buffer;
}

template <typename T>
struct ArraySlice
{
    T *m_array = nullptr;
    u64 m_size = 0;

    T &operator[](u64 idx)
    {
        ASSERT(idx < m_size);
        return m_array[idx];
    }

    T* begin()
    {
        return m_array;
    }

    T* end()
    {
        return m_array + m_size;
    }
};

template <typename T>
ArraySlice<T> arena_push_array(Arena *arena, u64 size)
{
    Slice allocation = arena_push_a(arena, sizeof(T) * size, alignof(T));
    return ArraySlice<T>{
        (T *)allocation.buffer,
        size};
}

#define ARENA_DEFER_CLEAR(arena)                      \
    Mark CONCAT(mark_, __LINE__) = arena_mark(arena); \
    DEFER { arena_clear_to_mark(arena, CONCAT(mark_, __LINE__)); };\
