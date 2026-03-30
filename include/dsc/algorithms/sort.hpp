#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"

// ── Sorting algorithms ────────────────────────────────────────────────────────
// All functions operate on raw pointer ranges [first, last).
// No iterators required — works directly on any contiguous array.
//
//  introsort    — O(n log n) worst-case, O(n) best (nearly sorted).
//                 Production-quality: quicksort → heapsort fallback → insertionsort.
//  merge_sort   — stable O(n log n); uses O(n) auxiliary memory.
//  insertion_sort — O(n²) worst, O(n) best; unbeatable for n ≤ ~16.
//  counting_sort  — O(n + k) for small integer ranges.
//  radix_sort     — O(nw) LSD for unsigned integers; w = word bits / 8.

namespace dsc {

// ── Default comparator (less-than) ───────────────────────────────────────────
struct Less {
    template<typename T>
    bool operator()(const T& a, const T& b) const noexcept { return a < b; }
};

// ── Insertion sort ────────────────────────────────────────────────────────────
// Best for small n (≤ 16) or nearly-sorted data.
/// @brief Sorts the range [first, last) using insertion sort.
/// @tparam T Element type.
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param cmp Comparator functor.
/// @complexity O(n²) worst-case, O(n) best-case for nearly sorted data.
/// @note Suitable for small ranges or data that is already mostly sorted.
template<typename T, typename Cmp = Less>
void insertion_sort(T* first, T* last, Cmp cmp = {}) noexcept {
    for (T* i = first + 1; i < last; ++i) {
        T key = traits::move(*i);
        T* j  = i - 1;
        while (j >= first && cmp(key, *j)) {
            *(j + 1) = traits::move(*j);
            --j;
        }
        *(j + 1) = traits::move(key);
    }
}

// ── Heap sort (used as introsort fallback) ────────────────────────────────────
namespace detail {

template<typename T, typename Cmp>
void sift_down(T* arr, isize root, isize n, Cmp& cmp) noexcept {
    while (true) {
        isize largest = root;
        isize l = 2 * root + 1;
        isize r = l + 1;
        if (l < n && cmp(arr[largest], arr[l])) largest = l;
        if (r < n && cmp(arr[largest], arr[r])) largest = r;
        if (largest == root) break;
        traits::swap(arr[root], arr[largest]);
        root = largest;
    }
}

template<typename T, typename Cmp>
void heap_sort_impl(T* first, T* last, Cmp& cmp) noexcept {
    isize n = last - first;
    // Build max-heap
    for (isize i = n / 2 - 1; i >= 0; --i) sift_down(first, i, n, cmp);
    // Extract
    for (isize i = n - 1; i > 0; --i) {
        traits::swap(first[0], first[i]);
        sift_down(first, 0, i, cmp);
    }
}

// Median of three pivot selection
template<typename T, typename Cmp>
T* median3(T* a, T* b, T* c, Cmp& cmp) noexcept {
    if (cmp(*b, *a)) traits::swap(*a, *b);
    if (cmp(*c, *a)) traits::swap(*a, *c);
    if (cmp(*c, *b)) traits::swap(*b, *c);
    return b;
}

template<typename T, typename Cmp>
T* partition(T* first, T* last, Cmp& cmp) noexcept {
    T* mid    = first + (last - first) / 2;
    T* pivot_p = median3(first, mid, last - 1, cmp);
    T  pivot   = traits::move(*pivot_p);
    traits::swap(*pivot_p, *(last - 1));

    T* i = first - 1;
    T* j = last - 1;
    for (;;) {
        while (cmp(*++i, pivot));
        while (j > first && cmp(pivot, *--j));
        if (i >= j) break;
        traits::swap(*i, *j);
    }
    traits::swap(*i, *(last - 1));
    return i;
}

// log2 floor (needed for introsort depth limit)
inline usize log2_floor(usize n) noexcept {
    usize r = 0;
    while (n > 1) { n >>= 1; ++r; }
    return r;
}

template<typename T, typename Cmp>
void introsort_impl(T* first, T* last, usize depth_limit, Cmp& cmp) noexcept {
    constexpr isize SMALL = 16;
    while (last - first > SMALL) {
        if (depth_limit == 0) {
            heap_sort_impl(first, last, cmp);
            return;
        }
        --depth_limit;
        T* pivot = partition(first, last, cmp);
        // Recurse on smaller partition; loop on larger (reduces stack depth)
        if (pivot - first < last - pivot) {
            introsort_impl(first, pivot, depth_limit, cmp);
            first = pivot + 1;
        } else {
            introsort_impl(pivot + 1, last, depth_limit, cmp);
            last = pivot;
        }
    }
}

} // namespace detail

// ── Introsort ─────────────────────────────────────────────────────────────────
// Worst-case O(n log n), average O(n log n), best O(n).
// Identical to the algorithm used in libstdc++ std::sort.
/// @brief Sorts the range [first, last) using introsort (hybrid quicksort + heapsort + insertionsort).
/// @tparam T Element type.
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param cmp Comparator functor.
/// @complexity O(n log n) worst-case, O(n log n) average, O(n) best-case.
/// @note Uses quicksort with heapsort fallback and insertionsort for small ranges.
template<typename T, typename Cmp = Less>
void introsort(T* first, T* last, Cmp cmp = {}) noexcept {
    if (last - first < 2) return;
    usize depth = 2 * detail::log2_floor(static_cast<usize>(last - first));
    detail::introsort_impl(first, last, depth, cmp);
    insertion_sort(first, last, cmp);
}

// Convenience alias
/// @brief Alias for `introsort`. Sorts the range [first, last) in ascending order.
/// @tparam T Element type.
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param cmp Comparator functor.
/// @complexity O(n log n) worst-case.
/// @see introsort
template<typename T, typename Cmp = Less>
void sort(T* first, T* last, Cmp cmp = {}) noexcept {
    introsort(first, last, cmp);
}

// ── Heap sort (standalone) ────────────────────────────────────────────────────
/// @brief Sorts the range [first, last) using heap sort.
/// @tparam T Element type.
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param cmp Comparator functor.
/// @complexity O(n log n) worst-case.
/// @note In-place sorting algorithm with O(1) auxiliary space.
template<typename T, typename Cmp = Less>
void heap_sort(T* first, T* last, Cmp cmp = {}) noexcept {
    detail::heap_sort_impl(first, last, cmp);
}

// ── Merge sort (stable) ───────────────────────────────────────────────────────
/// @brief Sorts the range [first, last) using stable merge sort.
/// @tparam T Element type.
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param cmp Comparator functor.
/// @complexity O(n log n) worst-case.
/// @note Stable sorting algorithm that preserves the relative order of equal elements.
///       Uses O(n) auxiliary space.
template<typename T, typename Cmp = Less>
void merge_sort(T* first, T* last, Cmp cmp = {}) {
    isize n = last - first;
    if (n < 2) return;
    if (n <= 16) { insertion_sort(first, last, cmp); return; }

    T* tmp = static_cast<T*>(::operator new(static_cast<usize>(n) * sizeof(T)));

    // Bottom-up iterative merge sort — avoids recursion stack
    for (isize width = 1; width < n; width *= 2) {
        for (isize lo = 0; lo < n; lo += 2 * width) {
            isize mid = lo + width      < n ? lo + width      : n;
            isize hi  = lo + 2 * width  < n ? lo + 2 * width  : n;

            // Merge [lo, mid) and [mid, hi) into tmp[lo, hi)
            isize i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                if (!cmp(first[j], first[i])) { new (tmp + k++) T(traits::move(first[i++])); }
                else                          { new (tmp + k++) T(traits::move(first[j++])); }
            }
            while (i < mid) new (tmp + k++) T(traits::move(first[i++]));
            while (j < hi)  new (tmp + k++) T(traits::move(first[j++]));

            // Move merged result back
            for (isize x = lo; x < hi; ++x) {
                first[x] = traits::move(tmp[x]);
                if constexpr (!traits::is_trivially_destructible_v<T>) tmp[x].~T();
            }
        }
    }
    ::operator delete(tmp);
}

// ── Counting sort ─────────────────────────────────────────────────────────────
// O(n + k). Only valid for non-negative integer types within [min, max].
/// @brief Sorts the range [first, last) using counting sort.
/// @tparam T Element type. Must be an integral type.
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @complexity O(n + k) where k is the range of values.
/// @pre Elements must be non-negative integers.
/// @note Only suitable for small ranges of integer values.
template<typename T>
void counting_sort(T* first, T* last) {
    static_assert(traits::is_integral_v<T>, "counting_sort requires integer type");
    if (last - first < 2) return;

    // Find range
    T lo = *first, hi = *first;
    for (T* p = first + 1; p < last; ++p) {
        if (*p < lo) lo = *p;
        if (*p > hi) hi = *p;
    }

    usize range = static_cast<usize>(hi - lo) + 1;
    usize* count = static_cast<usize*>(::operator new(range * sizeof(usize)));
    for (usize i = 0; i < range; ++i) count[i] = 0;
    for (T* p = first; p < last; ++p) ++count[static_cast<usize>(*p - lo)];

    T* out = first;
    for (usize i = 0; i < range; ++i)
        for (usize j = 0; j < count[i]; ++j)
            *out++ = static_cast<T>(static_cast<isize>(lo) + static_cast<isize>(i));

    ::operator delete(count);
}

// ── LSD Radix sort ────────────────────────────────────────────────────────────
// O(nw) for unsigned integer types. w = sizeof(T) * 8 / 8 passes.
// Stable. Very fast for large arrays of integers.
/// @brief Sorts the range [first, last) using least-significant-digit (LSD) radix sort.
/// @tparam T Element type. Must be an unsigned integral type.
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @complexity O(n * w) where w is the number of bytes in T.
/// @pre Elements must be unsigned integers.
/// @note Stable sorting algorithm, very efficient for large arrays of integers.
template<typename T>
void radix_sort(T* first, T* last) {
    static_assert(traits::is_unsigned_v<T>, "radix_sort requires unsigned integer type");
    isize n = last - first;
    if (n < 2) return;

    constexpr usize BITS = 8;
    constexpr usize BASE = 1u << BITS;    // 256
    constexpr usize MASK = BASE - 1;
    constexpr usize PASSES = sizeof(T);   // one pass per byte

    T* buf = static_cast<T*>(::operator new(static_cast<usize>(n) * sizeof(T)));
    usize cnt[BASE];

    for (usize pass = 0; pass < PASSES; ++pass) {
        usize shift = pass * BITS;
        for (usize i = 0; i < BASE; ++i) cnt[i] = 0;
        for (isize i = 0; i < n; ++i) ++cnt[(first[i] >> shift) & MASK];

        // Prefix sum
        usize total = 0;
        for (usize i = 0; i < BASE; ++i) { usize c = cnt[i]; cnt[i] = total; total += c; }

        for (isize i = 0; i < n; ++i) {
            usize digit = (first[i] >> shift) & MASK;
            buf[cnt[digit]++] = first[i];
        }
        // Copy back (swap buffers is more efficient but requires pointer swap)
        for (isize i = 0; i < n; ++i) first[i] = buf[i];
    }
    ::operator delete(buf);
}

} // namespace dsc
