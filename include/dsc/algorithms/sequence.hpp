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
template<typename T>
[[nodiscard]] Optional<usize[2]> two_sum_sorted(const T* arr, usize n, T target) noexcept {
    usize l = 0, r = n - 1;
    while (l < r) {
        T s = arr[l] + arr[r];
        if (s == target) {
            // Return via pair struct instead of C array (can't return C arrays)
            static usize res[2];
            res[0] = l; res[1] = r;
            return some(res);
        }
        else if (s < target) ++l;
        else                  --r;
    }
    return none_of<usize[2]>();
}

// ── Sliding window maximum ────────────────────────────────────────────────────
// For each window of size k, find the maximum. O(n) using monotone deque.
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
template<typename T>
[[nodiscard]] usize remove_duplicates(T* arr, usize n) noexcept {
    if (n == 0) return 0;
    usize j = 1;
    for (usize i = 1; i < n; ++i)
        if (!(arr[i] == arr[i-1])) arr[j++] = arr[i];
    return j;
}

// ── Merge two sorted arrays into dst ──────────────────────────────────────────
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
