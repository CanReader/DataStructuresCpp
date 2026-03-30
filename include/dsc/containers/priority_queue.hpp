#pragma once

#include "array.hpp"

namespace dsc {

// ── PriorityQueue<T, Cmp> ─────────────────────────────────────────────────────
// 4-ary max-heap (configurable comparator).
//
// Why 4-ary (D = 4) over binary?
//   • Each sift-down step processes D children per node instead of 2.
//   • Tree height = log₄ n ≈ 0.5 × log₂ n → half as many cache-line loads
//     when traversing from root to leaf (dominant cost on modern CPUs).
//   • Sift-up is rare (insert) — 4-ary costs the same as binary.
//   • Empirically 10-30 % faster than binary heap on modern hardware.
//
// Complexity:
//   push    : O(log n) amortized
//   pop     : O(log n)
//   top     : O(1)
//   build   : O(n)

/// @brief Priority queue backed by a 4-ary heap.
///
/// By default, the largest element is at the top (`MaxHeap` semantics). Supply a
/// reverse comparator to get a `MinHeap` — see the `MinHeap<T>` alias below.
///
/// **Why 4-ary?** Each sift-down visit processes 4 children per level instead of
/// 2, halving the tree height (log₄ n vs log₂ n). Since sift-down is the hot
/// path during `pop`, this dramatically reduces cache misses on modern hardware
/// (~10–30% faster than binary heap in practice).
///
/// Internal storage is a flat `Array<T>` — O(1) random access, cache-friendly.
///
/// @tparam T   Element type. Must be move-constructible and comparable via @p Cmp.
/// @tparam Cmp Binary predicate: `cmp(a, b)` returns `true` if `a` has *lower*
///             priority than `b` (i.e., `a < b` for max-heap). Defaults to `a < b`.
template<typename T,
         typename Cmp = decltype([](const T& a, const T& b){ return a < b; })>
class PriorityQueue {
    static constexpr usize D = 4;  // arity

public:
    /// @brief Default constructor. Creates an empty priority queue.
    /// @complexity O(1).
    PriorityQueue() = default;

    /// @brief Range constructor. Builds a heap from `[first, last)` in O(n).
    ///
    /// Uses Floyd's heapification algorithm — more efficient than inserting
    /// elements one by one (O(n) vs O(n log n)).
    /// @param first Pointer to the first element.
    /// @param last  Past-the-end pointer.
    /// @complexity O(n).
    PriorityQueue(const T* first, const T* last) {
        while (first != last) heap_.push_back(*first++);
        build();
    }

    // ── Core operations ───────────────────────────────────────────────────────

    /// @brief Pushes a copy of @p v onto the heap.
    /// @note May trigger a 1.5× reallocation of the underlying `Array<T>`.
    /// @complexity O(log n) amortised.
    void push(const T& v) { heap_.push_back(v);             sift_up(heap_.size() - 1); }

    /// @brief Pushes @p v onto the heap by move.
    /// @complexity O(log n) amortised.
    void push(T&& v)      { heap_.push_back(traits::move(v)); sift_up(heap_.size() - 1); }

    /// @brief Constructs a new element in-place and pushes it onto the heap.
    /// @tparam Args Constructor argument types.
    /// @param  args Arguments forwarded to `T`'s constructor.
    /// @return Mutable reference to the newly inserted element.
    /// @note May trigger a 1.5× reallocation of the underlying `Array<T>`.
    /// @complexity O(log n) amortised.
    template<typename... Args>
    T& emplace(Args&&... args) {
        T& ref = heap_.emplace_back(traits::forward<Args>(args)...);
        sift_up(heap_.size() - 1);
        return ref;
    }

    /// @brief Removes the top (highest-priority) element.
    ///
    /// Replaces the root with the last element and sifts it down.
    /// @note Has no effect if the queue is empty.
    /// @complexity O(log n).
    void pop() noexcept {
        if (heap_.empty()) return;
        heap_[0] = traits::move(heap_.back());
        heap_.pop_back();
        if (!heap_.empty()) sift_down(0);
    }

    /// @brief Removes and returns the top element by value.
    ///
    /// Equivalent to calling `top()` then `pop()`, but avoids an extra copy.
    /// @return The element that was at the top of the heap.
    /// @pre `!empty()`. Behaviour is undefined on an empty queue.
    /// @complexity O(log n).
    [[nodiscard]] T pop_value() {
        T v = traits::move(heap_[0]);
        pop();
        return v;
    }

    /// @brief Returns a const reference to the top (highest-priority) element.
    /// @pre `!empty()`. Behaviour is undefined on an empty queue.
    /// @complexity O(1).
    [[nodiscard]] const T& top() const noexcept { return heap_[0]; }

    /// @brief Returns a mutable reference to the top element.
    /// @pre `!empty()`. Behaviour is undefined on an empty queue.
    /// @warning Modifying the top element may violate the heap property.
    ///          Call `build()` afterwards if you need to restore it.
    /// @complexity O(1).
    [[nodiscard]] T&       top()       noexcept { return heap_[0]; }

    /// @brief Returns the number of elements in the queue.
    /// @complexity O(1).
    [[nodiscard]] usize size()  const noexcept { return heap_.size(); }

    /// @brief Returns `true` if the queue contains no elements.
    /// @complexity O(1).
    [[nodiscard]] bool  empty() const noexcept { return heap_.empty(); }

    /// @brief Removes all elements.
    /// @complexity O(n) for non-trivially destructible `T`; O(1) otherwise.
    void clear() noexcept { heap_.clear(); }

    // ── Bulk build from range ─────────────────────────────────────────────────

    /// @brief Restores the heap property over the entire internal array in O(n).
    ///
    /// Uses Floyd's bottom-up algorithm: sifts down every non-leaf node from
    /// the last non-leaf to the root. Useful after direct modification of
    /// the backing storage.
    /// @complexity O(n).
    void build() noexcept {
        if (heap_.size() < 2) return;
        usize n = heap_.size();
        // Floyd: sift-down from last non-leaf to root
        usize start = (n - 2) / D;
        while (start < n) {  // wraps at 0 — check with signed comparison
            sift_down(start);
            if (start == 0) break;
            --start;
        }
    }

private:
    Array<T> heap_;
    Cmp      cmp_{};

    void sift_up(usize i) noexcept {
        while (i > 0) {
            usize parent = (i - 1) / D;
            if (cmp_(heap_[parent], heap_[i])) {
                traits::swap(heap_[parent], heap_[i]);
                i = parent;
            } else break;
        }
    }

    void sift_down(usize i) noexcept {
        usize n = heap_.size();
        for (;;) {
            usize best = i;
            usize c0   = D * i + 1;
            usize lim  = c0 + D < n ? c0 + D : n;
            for (usize c = c0; c < lim; ++c) {
                if (cmp_(heap_[best], heap_[c])) best = c;
            }
            if (best == i) break;
            traits::swap(heap_[i], heap_[best]);
            i = best;
        }
    }
};

// ── MinHeap / MaxHeap aliases ─────────────────────────────────────────────────

/// @brief Max-heap alias for `PriorityQueue<T>`. Largest element is at the top.
template<typename T>
using MaxHeap = PriorityQueue<T>;  // default: max at top

/// @brief Min-heap alias for `PriorityQueue<T>`. Smallest element is at the top.
template<typename T>
using MinHeap = PriorityQueue<T, decltype([](const T& a, const T& b){ return a > b; })>;

} // namespace dsc
