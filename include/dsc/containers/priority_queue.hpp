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
template<typename T,
         typename Cmp = decltype([](const T& a, const T& b){ return a < b; })>
class PriorityQueue {
    static constexpr usize D = 4;  // arity

public:
    PriorityQueue() = default;

    // Build from an existing range in O(n) using Floyd's algorithm
    PriorityQueue(const T* first, const T* last) {
        while (first != last) heap_.push_back(*first++);
        build();
    }

    // ── Core operations ───────────────────────────────────────────────────────
    void push(const T& v) { heap_.push_back(v);             sift_up(heap_.size() - 1); }
    void push(T&& v)      { heap_.push_back(traits::move(v)); sift_up(heap_.size() - 1); }

    template<typename... Args>
    T& emplace(Args&&... args) {
        T& ref = heap_.emplace_back(traits::forward<Args>(args)...);
        sift_up(heap_.size() - 1);
        return ref;
    }

    void pop() noexcept {
        if (heap_.empty()) return;
        heap_[0] = traits::move(heap_.back());
        heap_.pop_back();
        if (!heap_.empty()) sift_down(0);
    }

    // Remove and return the top element
    [[nodiscard]] T pop_value() {
        T v = traits::move(heap_[0]);
        pop();
        return v;
    }

    [[nodiscard]] const T& top() const noexcept { return heap_[0]; }
    [[nodiscard]] T&       top()       noexcept { return heap_[0]; }

    [[nodiscard]] usize size()  const noexcept { return heap_.size(); }
    [[nodiscard]] bool  empty() const noexcept { return heap_.empty(); }

    void clear() noexcept { heap_.clear(); }

    // ── Bulk build from range ─────────────────────────────────────────────────
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
template<typename T>
using MaxHeap = PriorityQueue<T>;  // default: max at top

template<typename T>
using MinHeap = PriorityQueue<T, decltype([](const T& a, const T& b){ return a > b; })>;

} // namespace dsc
