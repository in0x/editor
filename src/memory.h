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
struct Array
{
    T* array = nullptr;
    s64 size = 0;
    s64 count = 0;

    T& operator[](s64 idx)
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
        return array + count;
    }

    bool is_valid() const
    {
        return array && size;
    }

    operator bool() const { return is_valid(); }
};

template <typename T>
struct Slice
{
    T const* array = nullptr;
    s64 count = 0;

    Slice() = default;

    template<s64 src_count>
    Slice(const T (&src)[src_count])
    {
        this->count = src_count;
        this->array = src;
    }

    T const& operator[](s64 idx) const
    {
        ASSERT(idx < count);
        return array[idx];
    }

    T const* begin() const
    {
        return array;
    }

    T const* end() const
    {
        return array + count;
    }
};

template <typename T, s64 len>
struct FixedArray : public Array<T>
{
    FixedArray()
    {
        this->array = buffer;
        this->size = len;
    }

    template<s64 src_count>
    FixedArray(const T (&src)[src_count])
        : FixedArray()
    {
        this->count = src_count;
        static_assert(src_count <= len);
        for (s64 i = 0; i < src_count; ++i) {
            buffer[i] = src[i];
        }
    }

    T buffer[len];
};

template <typename T>
T* array_push(Array<T>* arr, bool assert_on_fail = true)
{
    if (arr->count >= arr->size)
    {
        ASSERT_MSG(!assert_on_fail, "Exceeded array size when pushing");
        return nullptr;
    }

    return &(arr->operator[](arr->count++));    
}

template <typename T>
bool array_push(Array<T>* arr, T const& v, bool assert_on_fail = true)
{
    T* nv = array_push(arr, assert_on_fail);
    if (!nv) return false;
    *nv = v;
    return true;
}

template <typename T>
T* try_array_push(Array<T>* arr)
{
    return array_push(arr, false);
}

template <typename T>
bool try_array_push(Array<T>* arr, T const& v)
{
    return array_push(arr, v, false);
}

template <typename T>
Array<T> arena_push_array(Arena* arena, s64 size)
{
    void* allocation = arena_push_a(arena, sizeof(T) * size, alignof(T));
    return Array<T>{(T*)allocation, size, 0};
}

template <typename T>
Array<T> arena_push_array_with_count(Arena* arena, s64 size, s64 count)
{
    void* allocation = arena_push_a(arena, sizeof(T) * size, alignof(T));
    return Array<T>{(T*)allocation, size, count};
}

#define ARENA_DEFER_CLEAR(arena)                      \
    Mark CONCAT(mark_, __LINE__) = arena_mark(arena); \
    DEFER { arena_clear_to_mark(arena, CONCAT(mark_, __LINE__)); };\
    