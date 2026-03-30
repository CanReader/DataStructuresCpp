#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/optional.hpp"

// ── Search algorithms ─────────────────────────────────────────────────────────
// All operate on raw pointer ranges [first, last).
//
//  linear_search        — O(n); any range.
//  binary_search        — O(log n); sorted range.
//  lower_bound          — first position where val can be inserted (sorted range).
//  upper_bound          — last  position where val can be inserted (sorted range).
//  interpolation_search — O(log log n) avg for uniform distributions; O(n) worst.

namespace dsc {

// ── Linear search ─────────────────────────────────────────────────────────────
// Returns pointer to first element equal to val, or last if not found.
/// @brief Performs linear search for the first occurrence of val in the range [first, last).
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param val Value to search for.
/// @return Pointer to the first element equal to val, or last if not found.
/// @complexity O(n).
[[nodiscard]] T* linear_search(T* first, T* last, const U& val) noexcept {
    while (first != last) {
        if (*first == val) return first;
        ++first;
    }
    return last;
}

// Returns index, or -1 if not found.
/// @brief Performs linear search and returns the index of the first occurrence of val.
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param val Value to search for.
/// @return Index of the first element equal to val, or -1 if not found.
/// @complexity O(n).
template<typename T, typename U>
[[nodiscard]] isize linear_search_idx(const T* first, const T* last, const U& val) noexcept {
    for (const T* p = first; p != last; ++p)
        if (*p == val) return static_cast<isize>(p - first);
    return -1;
}

// ── Binary search ─────────────────────────────────────────────────────────────
// Precondition: [first, last) sorted ascending by cmp.
// Returns pointer to element equal to val, or nullptr if absent.
struct Less { template<typename T> bool operator()(const T& a, const T& b) const noexcept { return a < b; } };

/// @brief Performs binary search for val in the sorted range [first, last).
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the sorted range.
/// @param last Pointer to one past the last element of the sorted range.
/// @param val Value to search for.
/// @param cmp Comparator functor.
/// @return Pointer to the element equal to val, or nullptr if not found.
/// @pre The range [first, last) must be sorted according to cmp.
/// @complexity O(log n).
template<typename T, typename U, typename Cmp = Less>
[[nodiscard]] T* binary_search(T* first, T* last, const U& val, Cmp cmp = {}) noexcept {
    T* lo = first;
    T* hi = last;
    while (lo < hi) {
        T* mid = lo + (hi - lo) / 2;
        if (cmp(*mid, val))       lo = mid + 1;
        else if (cmp(val, *mid))  hi = mid;
        else                      return mid;
    }
    return nullptr;
}

// Returns true/false (cheaper — avoids the equality check)
/// @brief Checks if val exists in the sorted range [first, last) using binary search.
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the sorted range.
/// @param last Pointer to one past the last element of the sorted range.
/// @param val Value to search for.
/// @param cmp Comparator functor.
/// @return true if val is found, false otherwise.
/// @pre The range [first, last) must be sorted according to cmp.
/// @complexity O(log n).
template<typename T, typename U, typename Cmp = Less>
[[nodiscard]] bool binary_contains(const T* first, const T* last, const U& val, Cmp cmp = {}) noexcept {
    const T* lo = first;
    const T* hi = last;
    while (lo < hi) {
        const T* mid = lo + (hi - lo) / 2;
        if      (cmp(*mid, val)) lo = mid + 1;
        else if (cmp(val, *mid)) hi = mid;
        else                     return true;
    }
    return false;
}

// ── Lower bound ───────────────────────────────────────────────────────────────
// Returns pointer to the first element where !(cmp(*e, val)) — i.e., >= val.
/// @brief Finds the first position where val could be inserted to maintain sorted order.
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the sorted range.
/// @param last Pointer to one past the last element of the sorted range.
/// @param val Value to find the lower bound for.
/// @param cmp Comparator functor.
/// @return Pointer to the first element >= val, or last if no such element exists.
/// @pre The range [first, last) must be sorted according to cmp.
/// @complexity O(log n).
template<typename T, typename U, typename Cmp = Less>
[[nodiscard]] T* lower_bound(T* first, T* last, const U& val, Cmp cmp = {}) noexcept {
    T* lo = first; T* hi = last;
    while (lo < hi) {
        T* mid = lo + (hi - lo) / 2;
        if (cmp(*mid, val)) lo = mid + 1;
        else                hi = mid;
    }
    return lo;
}

// ── Upper bound ───────────────────────────────────────────────────────────────
// Returns pointer to the first element where cmp(val, *e) — i.e., > val.
/// @brief Finds the last position where val could be inserted to maintain sorted order.
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the sorted range.
/// @param last Pointer to one past the last element of the sorted range.
/// @param val Value to find the upper bound for.
/// @param cmp Comparator functor.
/// @return Pointer to the first element > val, or last if no such element exists.
/// @pre The range [first, last) must be sorted according to cmp.
/// @complexity O(log n).
template<typename T, typename U, typename Cmp = Less>
[[nodiscard]] T* upper_bound(T* first, T* last, const U& val, Cmp cmp = {}) noexcept {
    T* lo = first; T* hi = last;
    while (lo < hi) {
        T* mid = lo + (hi - lo) / 2;
        if (cmp(val, *mid)) hi = mid;
        else                lo = mid + 1;
    }
    return lo;
}

// ── Equal range ───────────────────────────────────────────────────────────────
// Returns {lower_bound, upper_bound} pair as two pointers.
template<typename T, typename U, typename Cmp = Less>
struct Range { T* first; T* last; };

/// @brief Finds the range of elements equal to val in the sorted range [first, last).
/// @tparam T Element type.
/// @tparam U Value type (must be comparable with T).
/// @tparam Cmp Comparator type. Defaults to `Less` (ascending order).
/// @param first Pointer to the first element of the sorted range.
/// @param last Pointer to one past the last element of the sorted range.
/// @param val Value to find the equal range for.
/// @param cmp Comparator functor.
/// @return A Range struct containing pointers to the first and one-past-last elements equal to val.
/// @pre The range [first, last) must be sorted according to cmp.
/// @complexity O(log n).
template<typename T, typename U, typename Cmp = Less>
[[nodiscard]] Range<T, U> equal_range(T* first, T* last, const U& val, Cmp cmp = {}) noexcept {
    return { lower_bound(first, last, val, cmp), upper_bound(first, last, val, cmp) };
}

// ── Interpolation search ──────────────────────────────────────────────────────
// O(log log n) average for uniformly distributed data; O(n) worst case.
// Precondition: [first, last) sorted ascending. T must support arithmetic.
/// @brief Performs interpolation search for val in the sorted range [first, last).
/// @tparam T Element type. Must support arithmetic operations.
/// @param first Pointer to the first element of the sorted range.
/// @param last Pointer to one past the last element of the sorted range.
/// @param val Value to search for.
/// @return Pointer to the element equal to val, or last if not found.
/// @pre The range [first, last) must be sorted in ascending order.
/// @pre Elements must support arithmetic operations.
/// @complexity O(log log n) average for uniformly distributed data, O(n) worst-case.
/// @note Most efficient when data is uniformly distributed.
template<typename T>
[[nodiscard]] T* interpolation_search(T* first, T* last, const T& val) noexcept {
    T* lo = first;
    T* hi = last - 1;
    while (lo <= hi && val >= *lo && val <= *hi) {
        if (lo == hi) { return (*lo == val) ? lo : last; }
        // Estimate position by linear interpolation
        usize range = static_cast<usize>(*hi - *lo);
        if (range == 0) break;
        usize offset = static_cast<usize>((val - *lo)) * (static_cast<usize>(hi - lo)) / range;
        T* mid = lo + offset;
        if (*mid == val) return mid;
        if (*mid < val)  lo = mid + 1;
        else             hi = mid - 1;
    }
    return last;  // not found
}

} // namespace dsc
