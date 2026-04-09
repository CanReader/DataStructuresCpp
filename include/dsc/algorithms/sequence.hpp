#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/optional.hpp"
#include "../containers/array.hpp"
#include "../containers/stack.hpp"

// ── Sequence / array algorithms ───────────────────────────────────────────────
// Classic problems on contiguous arrays and general sequences.

namespace dsc {

// ── Kadane's algorithm — Maximum Subarray Sum ────────────────────────────────
// O(n). Returns the maximum sum and the subarray's [lo, hi] indices.
template<typename T>
struct SubarrayResult { T sum; usize lo; usize hi; };

/// @brief Finds the maximum subarray sum using Kadane's algorithm.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @return SubarrayResult containing the maximum sum and indices [lo, hi].
/// @complexity O(n).
template<typename T>
[[nodiscard]] SubarrayResult<T> max_subarray(const T* arr, usize n) noexcept {
    if (n == 0) return {T{}, 0, 0};
    T    best = arr[0], cur = arr[0];
    usize bl = 0, bh = 0, cl = 0;
    for (usize i = 1; i < n; ++i) {
        if (cur + arr[i] < arr[i]) { cur = arr[i]; cl = i; }
        else                       { cur += arr[i]; }
        if (cur > best) { best = cur; bl = cl; bh = i; }
    }
    return {best, bl, bh};
}

// ── Two-pointer: sorted pair with target sum ──────────────────────────────────
// Find pair (i, j) s.t. arr[i]+arr[j] == target in sorted array. O(n).
/// @brief Finds a pair with target sum in a sorted array using two pointers.
/// @tparam T Element type.
/// @param arr Sorted input array.
/// @param n Array size.
/// @param target Target sum.
/// @return Optional containing a pair of indices where arr[i] + arr[j] == target, or none.
/// @pre Array must be sorted in ascending order.
/// @complexity O(n).
template<typename T>
struct IndexPair { usize first; usize second; };

template<typename T>
[[nodiscard]] Optional<IndexPair<T>> two_sum_sorted(const T* arr, usize n, T target) noexcept {
    usize l = 0, r = n - 1;
    while (l < r) {
        T s = arr[l] + arr[r];
        if (s == target) {
            return some(IndexPair<T>{l, r});
        }
        else if (s < target) ++l;
        else                  --r;
    }
    return none_of<IndexPair<T>>();
}

// ── Sliding window maximum ────────────────────────────────────────────────────
// For each window of size k, find the maximum. O(n) using monotone deque.
/// @brief Computes sliding window maximums for each window of size k.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @param k Window size.
/// @return Array of maximums for each window.
/// @complexity O(n).
template<typename T>
Array<T> sliding_window_max(const T* arr, usize n, usize k) {
    Array<T>     result;
    Array<usize> dq;  // monotone decreasing deque of indices
    result.reserve(n - k + 1);

    for (usize i = 0; i < n; ++i) {
        // Remove out-of-window indices
        while (!dq.empty() && dq.front() + k <= i)
            dq.erase(dq.begin());
        // Maintain decreasing order
        while (!dq.empty() && arr[dq.back()] <= arr[i])
            dq.pop_back();
        dq.push_back(i);
        if (i + 1 >= k) result.push_back(arr[dq.front()]);
    }
    return result;
}

// ── Sliding window minimum ─────────────────────────────────────────────────────
/// @brief Computes sliding window minimums for each window of size k.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @param k Window size.
/// @return Array of minimums for each window.
/// @complexity O(n).
template<typename T>
Array<T> sliding_window_min(const T* arr, usize n, usize k) {
    Array<T>     result;
    Array<usize> dq;
    result.reserve(n - k + 1);
    for (usize i = 0; i < n; ++i) {
        while (!dq.empty() && dq.front() + k <= i) dq.erase(dq.begin());
        while (!dq.empty() && arr[dq.back()] >= arr[i]) dq.pop_back();
        dq.push_back(i);
        if (i + 1 >= k) result.push_back(arr[dq.front()]);
    }
    return result;
}

// ── Next permutation ──────────────────────────────────────────────────────────
// Rearranges arr in-place to the next lexicographic permutation. O(n).
// Returns false if already the last permutation (sorted descending).
/// @brief Generates the next permutation in lexicographic order.
/// @tparam T Element type.
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @return true if a next permutation exists, false if already the last.
/// @complexity O(n).
template<typename T>
bool next_permutation(T* first, T* last) noexcept {
    if (last - first < 2) return false;
    T* i = last - 1;
    while (i > first && !(*(i-1) < *i)) --i;
    if (i == first) {
        // Reverse to get smallest permutation
        T* l = first, *r = last - 1;
        while (l < r) traits::swap(*l++, *r--);
        return false;
    }
    --i;
    T* j = last - 1;
    while (!(*i < *j)) --j;
    traits::swap(*i, *j);
    T* l = i + 1, *r = last - 1;
    while (l < r) traits::swap(*l++, *r--);
    return true;
}

// ── Previous permutation ──────────────────────────────────────────────────────
/// @brief Generates the previous permutation in lexicographic order.
/// @tparam T Element type.
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @return true if a previous permutation exists, false if already the first.
/// @complexity O(n).
template<typename T>
bool prev_permutation(T* first, T* last) noexcept {
    if (last - first < 2) return false;
    T* i = last - 1;
    while (i > first && !(*i < *(i-1))) --i;
    if (i == first) {
        T* l = first, *r = last - 1;
        while (l < r) traits::swap(*l++, *r--);
        return false;
    }
    --i;
    T* j = last - 1;
    while (!(*j < *i)) --j;
    traits::swap(*i, *j);
    T* l = i + 1, *r = last - 1;
    while (l < r) traits::swap(*l++, *r--);
    return true;
}

// ── Fisher-Yates shuffle ──────────────────────────────────────────────────────
// Uniform random shuffle in O(n). Needs an rng function: usize(usize limit).
/// @brief Shuffles the range [first, last) using Fisher-Yates algorithm.
/// @tparam T Element type.
/// @tparam Rng Random number generator type.
/// @param first Pointer to the first element of the range.
/// @param last Pointer to one past the last element of the range.
/// @param rng Random number generator function that takes a limit and returns usize in [0, limit).
/// @complexity O(n).
template<typename T, typename Rng>
void shuffle(T* first, T* last, Rng&& rng) noexcept {
    usize n = static_cast<usize>(last - first);
    for (usize i = n; i-- > 1;) {
        usize j = rng(i + 1);  // random in [0, i]
        traits::swap(first[i], first[j]);
    }
}

// ── Boyer-Moore majority vote ──────────────────────────────────────────────────
// Find candidate for majority element (appears > n/2 times) in O(n), O(1) space.
// Returns the candidate; caller must verify if actual majority is required.
/// @brief Finds a candidate for the majority element using Boyer-Moore voting algorithm.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @return Candidate for majority element (appears more than n/2 times).
/// @complexity O(n), O(1) space.
/// @note Caller must verify if the candidate is actually a majority.
template<typename T>
[[nodiscard]] T majority_candidate(const T* arr, usize n) noexcept {
    T    candidate = arr[0];
    isize count    = 1;
    for (usize i = 1; i < n; ++i) {
        if (count == 0) { candidate = arr[i]; count = 1; }
        else if (arr[i] == candidate) ++count;
        else --count;
    }
    return candidate;
}

// ── Longest Increasing Subsequence (LIS) ────────────────────────────────────
// Returns the length of the LIS. O(n log n) using patience sorting.
/// @brief Computes the length of the longest increasing subsequence.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @return Length of the longest increasing subsequence.
/// @complexity O(n log n).
template<typename T>
[[nodiscard]] usize lis_length(const T* arr, usize n) noexcept {
    Array<T> tails;  // tails[i] = smallest tail of all increasing subseqs of length i+1
    for (usize i = 0; i < n; ++i) {
        // Binary search for leftmost position where tails[pos] >= arr[i]
        usize lo = 0, hi = tails.size();
        while (lo < hi) {
            usize mid = (lo + hi) / 2;
            if (tails[mid] < arr[i]) lo = mid + 1;
            else                     hi = mid;
        }
        if (lo == tails.size()) tails.push_back(arr[i]);
        else                    tails[lo] = arr[i];
    }
    return tails.size();
}

// ── Monotone stack: next greater element ─────────────────────────────────────
// For each element, find the index of the next greater element. O(n).
// Returns n if no greater element exists to the right.
/// @brief Finds the index of the next greater element for each position.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @return Array where result[i] is the index of the next greater element after i, or n if none.
/// @complexity O(n).
template<typename T>
Array<usize> next_greater(const T* arr, usize n) {
    Array<usize> result(n, n);
    Stack<usize> stk;
    for (usize i = 0; i < n; ++i) {
        while (!stk.empty() && arr[stk.top()] < arr[i]) {
            result[stk.top()] = i;
            stk.pop();
        }
        stk.push(i);
    }
    return result;
}

// ── Monotone stack: previous greater element ──────────────────────────────────
/// @brief Finds the index of the previous greater element for each position.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @return Array where result[i] is the index of the previous greater element before i, or n if none.
/// @complexity O(n).
template<typename T>
Array<usize> prev_greater(const T* arr, usize n) {
    Array<usize> result(n, n);
    Stack<usize> stk;
    for (usize i = n; i-- > 0;) {
        while (!stk.empty() && arr[stk.top()] < arr[i]) {
            result[stk.top()] = i;
            stk.pop();
        }
        stk.push(i);
    }
    return result;
}

// ── Largest rectangle in histogram ────────────────────────────────────────────
// O(n) using monotone stack. Classic.
/// @brief Computes the area of the largest rectangle in a histogram.
/// @param heights Array of bar heights.
/// @param n Number of bars.
/// @return Area of the largest rectangle.
/// @complexity O(n).
[[nodiscard]] inline u64 largest_rect_histogram(const u64* heights, usize n) noexcept {
    Stack<usize> stk;
    u64 max_area = 0;
    for (usize i = 0; i <= n; ++i) {
        u64 h = i == n ? 0 : heights[i];
        while (!stk.empty() && heights[stk.top()] > h) {
            u64    height = heights[stk.pop_value()];
            usize  width  = stk.empty() ? i : i - stk.top() - 1;
            u64    area   = height * width;
            if (area > max_area) max_area = area;
        }
        stk.push(i);
    }
    return max_area;
}

// ── Dutch National Flag (3-way partition) ────────────────────────────────────
// Partition arr into [< pivot, == pivot, > pivot] in O(n), O(1) space.
/// @brief Partitions the array into three parts: less than, equal to, and greater than pivot.
/// @tparam T Element type.
/// @param arr Input array (modified in-place).
/// @param n Array size.
/// @param pivot Pivot value.
/// @complexity O(n), O(1) space.
template<typename T>
void dutch_national_flag(T* arr, usize n, const T& pivot) noexcept {
    usize lo = 0, mid = 0, hi = n;
    while (mid < hi) {
        if (arr[mid] < pivot)       traits::swap(arr[lo++], arr[mid++]);
        else if (arr[mid] == pivot) ++mid;
        else                        traits::swap(arr[mid], arr[--hi]);
    }
}

// ── Rotate array left by k positions ──────────────────────────────────────────
// Uses the three-reverse trick. O(n), O(1) space.
/// @brief Rotates the array left by k positions in-place.
/// @tparam T Element type.
/// @param arr Input array (modified in-place).
/// @param n Array size.
/// @param k Number of positions to rotate.
/// @complexity O(n), O(1) space.
template<typename T>
void rotate_left(T* arr, usize n, usize k) noexcept {
    k %= n;
    if (k == 0) return;
    auto rev = [](T* lo, T* hi) noexcept {
        while (lo < hi) traits::swap(*lo++, *--hi);
    };
    rev(arr,     arr + k);
    rev(arr + k, arr + n);
    rev(arr,     arr + n);
}

// ── Remove duplicates from sorted array ───────────────────────────────────────
// Returns new length (elements after are unspecified). O(n).
/// @brief Removes duplicates from a sorted array in-place.
/// @tparam T Element type.
/// @param arr Sorted input array (modified in-place).
/// @param n Array size.
/// @return New length of the array after removing duplicates.
/// @pre Array must be sorted.
/// @complexity O(n).
template<typename T>
[[nodiscard]] usize remove_duplicates(T* arr, usize n) noexcept {
    if (n == 0) return 0;
    usize j = 1;
    for (usize i = 1; i < n; ++i)
        if (!(arr[i] == arr[i-1])) arr[j++] = arr[i];
    return j;
}

// ── Merge two sorted arrays into dst ──────────────────────────────────────────
/// @brief Merges two sorted arrays into a destination array.
/// @tparam T Element type.
/// @param a First sorted array.
/// @param na Size of first array.
/// @param b Second sorted array.
/// @param nb Size of second array.
/// @param dst Destination array (must have size na + nb).
/// @complexity O(na + nb).
template<typename T>
void merge_sorted(const T* a, usize na, const T* b, usize nb, T* dst) noexcept {
    usize i = 0, j = 0, k = 0;
    while (i < na && j < nb) {
        if (b[j] < a[i]) dst[k++] = b[j++];
        else              dst[k++] = a[i++];
    }
    while (i < na) dst[k++] = a[i++];
    while (j < nb) dst[k++] = b[j++];
}

// ── Inversion count using merge sort ──────────────────────────────────────────
// Count pairs (i, j) with i < j and arr[i] > arr[j]. O(n log n).
/// @brief Counts the number of inversions in the array using merge sort.
/// @tparam T Element type.
/// @param arr Input array (modified in-place during sorting).
/// @param n Array size.
/// @return Number of inversions.
/// @complexity O(n log n).
template<typename T>
u64 count_inversions(T* arr, usize n) {
    if (n < 2) return 0;
    usize mid = n / 2;
    u64 inv = count_inversions(arr, mid) + count_inversions(arr + mid, n - mid);
    T* tmp = Allocator<T>::allocate(n);
    usize i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        if (!(arr[j] < arr[i])) { Allocator<T>::construct(tmp + k++, arr[i++]); }
        else {
            inv += mid - i;
            Allocator<T>::construct(tmp + k++, arr[j++]);
        }
    }
    while (i < mid) Allocator<T>::construct(tmp + k++, arr[i++]);
    while (j < n)   Allocator<T>::construct(tmp + k++, arr[j++]);
    for (usize x = 0; x < n; ++x) { arr[x] = traits::move(tmp[x]); Allocator<T>::destroy(tmp + x); }
    Allocator<T>::deallocate(tmp, n);
    return inv;
}

// ── 0/1 Knapsack ──────────────────────────────────────────────────────────────
// dp[i][w] = max value using items[0..i] with capacity w. O(nW) time+space.
// Space-optimised to O(W) using rolling array.
struct KnapsackItem { u64 weight; u64 value; };

/// @brief Solves the 0/1 knapsack problem.
/// @param items Array of items with weight and value.
/// @param n Number of items.
/// @param capacity Knapsack capacity.
/// @return Maximum value that can be achieved.
/// @complexity O(n * capacity), O(capacity) space.
[[nodiscard]] inline u64 knapsack_01(const KnapsackItem* items, usize n, u64 capacity) {
    Array<u64> dp(static_cast<usize>(capacity) + 1, 0ULL);
    for (usize i = 0; i < n; ++i) {
        // Traverse backwards to prevent using same item twice
        for (u64 w = capacity; w >= items[i].weight; --w) {
            u64 with = dp[static_cast<usize>(w - items[i].weight)] + items[i].value;
            if (with > dp[static_cast<usize>(w)]) dp[static_cast<usize>(w)] = with;
        }
    }
    return dp[static_cast<usize>(capacity)];
}

// ── Coin change — minimum coins ────────────────────────────────────────────────
// Returns minimum number of coins to make `amount`, or usize(-1) if impossible.
/// @brief Finds the minimum number of coins needed to make a given amount.
/// @param coins Array of coin denominations.
/// @param n_coins Number of coin types.
/// @param amount Target amount.
/// @return Minimum number of coins, or usize(-1) if impossible.
/// @complexity O(amount * n_coins).
[[nodiscard]] inline usize coin_change(const u64* coins, usize n_coins, u64 amount) {
    const usize INF_VAL = usize(-1) >> 1;
    Array<usize> dp(static_cast<usize>(amount) + 1, INF_VAL);
    dp[0] = 0;
    for (u64 a = 1; a <= amount; ++a) {
        for (usize j = 0; j < n_coins; ++j) {
            if (coins[j] <= a && dp[static_cast<usize>(a - coins[j])] != INF_VAL) {
                usize cand = dp[static_cast<usize>(a - coins[j])] + 1;
                if (cand < dp[static_cast<usize>(a)]) dp[static_cast<usize>(a)] = cand;
            }
        }
    }
    return dp[static_cast<usize>(amount)] == INF_VAL ? usize(-1) : dp[static_cast<usize>(amount)];
}

} // namespace dsc
