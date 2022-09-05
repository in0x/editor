#pragma once

#include "core.h"

template <typename T>
struct Array
{
    T* data   = nullptr;
    u32 size  = 0;
    u32 count = 0;

    T& operator[](u64 idx)
    {
        ASSERT(idx < count);
        return data[idx];
    }
};

template <typename T>
void array_alloc(Array<T>* arr, u32 size)
{
    ASSERT(arr->data == nullptr);
    ASSERT(arr->size == 0);

    arr->data = new T[size];
    arr->size = size;
    arr->count = 0;
}

// Increases the array's number of elements by 1, appending a copy of val to the current end.
template <typename T>
void array_add(Array<T>* arr, T* val)
{
    ASSERT(arr->data != nullptr);
    ASSERT(arr->count < arr->size);

    arr->data[arr->count++] = *val;
}

// Increases the array's number of elements by n, appending copies of the n values pointed to by
// n to the current end.
template <typename T>
void array_add_n(Array<T>* arr, T* vals, u32 n)
{
    ASSERT(arr->data != nullptr);
    ASSERT((arr->count + n) <= arr->size);

    memcpy_s(arr->data + arr->count, arr->size - arr->count, vals, sizeof(T) * n);
    arr->count += n;
}

template <typename T>
void array_set_count(Array<T>* arr, u32 count)
{
    ASSERT(arr->data != nullptr);
    ASSERT(count <= arr->size);

    arr->count = count;
}

template <typename T>
void array_free(Array<T>* arr)
{
    delete[] arr->data;
    arr->size  = 0;
    arr->count = 0;
}

template <typename T>
bool array_empty(Array<T> arr)
{
    return arr.count == 0;
}