#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../containers/array.hpp"

// ── Utility algorithms ────────────────────────────────────────────────────────
// General-purpose algorithms for common tasks.

namespace dsc {

// ── Prefix Sum / Cumulative Sum ───────────────────────────────────────────────
/// @brief Computes prefix sums in-place.
/// @param arr Input/output array.
/// @param n Array size.
/// @complexity O(n).
template<typename T>
void prefix_sum(T* arr, usize n) noexcept {
    for (usize i = 1; i < n; ++i) {
        arr[i] += arr[i - 1];
    }
}

// ── Difference Array ──────────────────────────────────────────────────────────
/// @brief Converts array to difference array for efficient range updates.
/// @param arr Input array.
/// @param n Array size.
/// @return Difference array (diff[i] = arr[i] - arr[i-1]).
/// @complexity O(n).
template<typename T>
Array<T> to_difference_array(const T* arr, usize n) noexcept {
    Array<T> diff(n, T{});
    diff[0] = arr[0];
    for (usize i = 1; i < n; ++i) {
        diff[i] = arr[i] - arr[i - 1];
    }
    return diff;
}

/// @brief Converts difference array back to normal array.
/// @param diff Difference array.
/// @param n Array size.
/// @return Original array (computed via prefix sum).
/// @complexity O(n).
template<typename T>
Array<T> from_difference_array(const T* diff, usize n) noexcept {
    Array<T> arr(n, T{});
    arr[0] = diff[0];
    for (usize i = 1; i < n; ++i) {
        arr[i] = arr[i - 1] + diff[i];
    }
    return arr;
}

// ── Array Rotation ───────────────────────────────────────────────────────────
/// @brief Rotates array left by k positions using reversal algorithm.
/// @param arr Input array.
/// @param n Array size.
/// @param k Rotation amount.
/// @complexity O(n).
template<typename T>
void rotate_left(T* arr, usize n, usize k) noexcept {
    if (n == 0) return;
    k %= n;
    
    // Reverse [0, k)
    for (usize i = 0; i < k / 2; ++i) {
        traits::swap(arr[i], arr[k - 1 - i]);
    }
    
    // Reverse [k, n)
    for (usize i = k; i < k + (n - k) / 2; ++i) {
        traits::swap(arr[i], arr[n - 1 - (i - k)]);
    }
    
    // Reverse [0, n)
    for (usize i = 0; i < n / 2; ++i) {
        traits::swap(arr[i], arr[n - 1 - i]);
    }
}

/// @brief Rotates array right by k positions.
/// @param arr Input array.
/// @param n Array size.
/// @param k Rotation amount.
/// @complexity O(n).
template<typename T>
void rotate_right(T* arr, usize n, usize k) noexcept {
    if (n > 0) {
        k %= n;
        rotate_left(arr, n, n - k);
    }
}

// ── Array Reverse ────────────────────────────────────────────────────────────
/// @brief Reverses an array in-place.
/// @param arr Input array.
/// @param n Array size.
/// @complexity O(n).
template<typename T>
void reverse(T* arr, usize n) noexcept {
    for (usize i = 0; i < n / 2; ++i) {
        traits::swap(arr[i], arr[n - 1 - i]);
    }
}

// ── Duplicate Removal ────────────────────────────────────────────────────────
/// @brief Removes duplicates from a sorted array (in-place).
/// @param arr Sorted input array.
/// @param n Array size.
/// @return New size after removing duplicates.
/// @complexity O(n).
template<typename T>
usize remove_duplicates(T* arr, usize n) noexcept {
    if (n <= 1) return n;
    usize write = 0;
    for (usize i = 1; i < n; ++i) {
        if (arr[i] != arr[write]) {
            arr[++write] = arr[i];
        }
    }
    return write + 1;
}

// ── Count Occurrences ─────────────────────────────────────────────────────────
/// @brief Counts occurrences of each unique value.
/// @param arr Input array.
/// @param n Array size.
/// @param out Output: array where out[i].first = value, out[i].second = count.
/// @return Number of unique elements.
/// @complexity O(n²) (naive; for sorted use two-pointer).
template<typename T>
usize count_occurrences(const T* arr, usize n, Array<Array<T>>& out) noexcept {
    out.clear();
    for (usize i = 0; i < n; ++i) {
        bool found = false;
        for (usize j = 0; j < out.size(); ++j) {
            if (out[j][0] == arr[i]) {
                out[j][1]++;
                found = true;
                break;
            }
        }
        if (!found) {
            Array<T> pair(2);
            pair[0] = arr[i];
            pair[1] = 1;
            out.push_back(pair);
        }
    }
    return out.size();
}

// ── Merge Sorted Arrays ───────────────────────────────────────────────────────
/// @brief Merges two sorted arrays into a new array.
/// @param a First sorted array.
/// @param m Size of first array.
/// @param b Second sorted array.
/// @param n Size of second array.
/// @return Merged sorted array.
/// @complexity O(m + n).
template<typename T>
Array<T> merge_sorted(const T* a, usize m, const T* b, usize n) noexcept {
    Array<T> result(m + n);
    usize i = 0, j = 0, k = 0;
    
    while (i < m && j < n) {
        if (a[i] <= b[j]) {
            result[k++] = a[i++];
        } else {
            result[k++] = b[j++];
        }
    }
    
    while (i < m) result[k++] = a[i++];
    while (j < n) result[k++] = b[j++];
    
    return result;
}

// ── Partition (for quicksort-like operations) ────────────────────────────────
/// @brief Partitions array around a pivot value.
/// @param arr Input array.
/// @param n Array size.
/// @param pivot Pivot value.
/// @return Index of first element >= pivot.
/// @complexity O(n).
template<typename T>
usize partition(T* arr, usize n, const T& pivot) noexcept {
    usize left = 0, right = n;
    while (left < right) {
        while (left < right && arr[left] < pivot) ++left;
        while (left < right && arr[right - 1] >= pivot) --right;
        if (left < right) {
            traits::swap(arr[left], arr[right - 1]);
            ++left; --right;
        }
    }
    return left;
}

// ── Array Shuffle (Fisher-Yates) ──────────────────────────────────────────────
/// @brief Shuffles an array using Fisher-Yates algorithm (requires seeded RNG).
/// @param arr Input array.
/// @param n Array size.
/// @complexity O(n).
template<typename T>
void shuffle(T* arr, usize n, u64& rng_state) noexcept {
    for (usize i = n - 1; i > 0; --i) {
        // Simple LCG: rng_state = (rng_state * 1103515245 + 12345) & 0x7fffffff
        rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
        usize j = (rng_state >> 33) % (i + 1);
        traits::swap(arr[i], arr[j]);
    }
}

// ── Fill Array ────────────────────────────────────────────────────────────────
/// @brief Fills array with a value.
/// @param arr Output array.
/// @param n Array size.
/// @param val Value to fill.
/// @complexity O(n).
template<typename T>
void fill(T* arr, usize n, const T& val) noexcept {
    for (usize i = 0; i < n; ++i) arr[i] = val;
}

// ── Copy Array ────────────────────────────────────────────────────────────────
/// @brief Copies array elements.
/// @param dst Destination array.
/// @param src Source array.
/// @param n Number of elements to copy.
/// @complexity O(n).
template<typename T>
void copy(T* dst, const T* src, usize n) noexcept {
    for (usize i = 0; i < n; ++i) dst[i] = src[i];
}

// ── Minimum and Maximum ───────────────────────────────────────────────────────
/// @brief Finds minimum and maximum in an array.
/// @param arr Input array.
/// @param n Array size.
/// @param min Output: minimum value.
/// @param max Output: maximum value.
/// @complexity O(n).
template<typename T>
void min_max(const T* arr, usize n, T& min_val, T& max_val) noexcept {
    if (n == 0) return;
    min_val = max_val = arr[0];
    for (usize i = 1; i < n; ++i) {
        if (arr[i] < min_val) min_val = arr[i];
        if (arr[i] > max_val) max_val = arr[i];
    }
}

} // namespace dsc
