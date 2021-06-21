#pragma once
#include <stddef.h>

#include <algorithm>

namespace jc
{
    template<typename T> T* upper_bound(T* first, T* last, const T& value);
    template<typename T> T* lower_bound(T* first, T* last, const T& value);

    template<typename T, typename Value, typename Compare>
    T* upper_bound(T* first, T* last, const Value& value, Compare& comp);

    template<typename T, typename Value, typename Compare>
    T* lower_bound(T* first, T* last, const Value& value, Compare& comp);

    template<typename T> size_t lower_bound_offset(T* first, size_t size, const T& val);
    template<typename T> size_t upper_bound_offset(T* first, size_t size, const T& val);
}

#if defined(JC_ALGORITHM_IMPLEMENTATION)

namespace jc
{

template<typename T>
size_t upper_bound_offset(T* first, size_t size, const T& val)
{
    size_t low = 0;
    while (size > 0) {
        size_t half = size >> 1;
        size_t middle = low + half;

        if (val < first[middle])
        {
            size = half;
        }
        else
        {
            low = middle + 1;
            size = size - half - 1;
        }
    }
    return low;
}

template<typename T>
T* upper_bound(T* first, T* last, const T& val)
{
    size_t size = (size_t)(last - first);
    while (size > 0) {
        size_t half = size >> 1;
        T* middle = first + half;

        if (val < *middle)
        {
            size = half;
        }
        else
        {
            first = middle + 1;
            size = size - half - 1;
        }
    }
    return first;
}

template<typename T, typename Value, typename Compare>
T* upper_bound(T* first, T* last, const Value& val, Compare& comp)
{
    size_t size = (size_t)(last - first);
    while (size > 0) {
        size_t half = size >> 1;
        T* middle = first + half;

        if (comp(val, *middle))
        {
            size = half;
        }
        else
        {
            first = middle + 1;
            size = size - half - 1;
        }
    }
    return first;
}

template<typename T>
size_t lower_bound_offset(T* first, size_t size, const T& val)
{
    size_t low = 0;
    while (size > 0) {
        size_t half = size >> 1;
        size_t middle = low + half;

        if (first[middle] < val)
        {
            low = middle + 1;
            size = size - half - 1;
        }
        else
        {
            size = half;
        }
    }
    return low;
}


template<typename T>
T* lower_bound(T* first, T* last, const T& val)
{
    size_t size = (size_t)(last - first);
    while (size > 0) {
        size_t half = size >> 1;
        T* middle = first + half;

        if (*middle < val)
        {
            first = middle + 1;
            size = size - half - 1;
        }
        else
        {
            size = half;
        }
    }
    return first;
}

template<typename T, typename VAL, typename Compare>
T* lower_bound(T* first, T* last, const VAL& val, Compare& comp)
{
    size_t size = (size_t)(last - first);
    while (size > 0) {
        size_t half = size >> 1;
        T* middle = first + half;

        if (comp(*middle, val))
        {
            first = middle + 1;
            size = size - half - 1;
        }
        else
        {
            size = half;
        }
    }
    return first;
}

} // namespace

#endif
