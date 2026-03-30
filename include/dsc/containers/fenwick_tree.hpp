#pragma once

#include "../core/primitives.hpp"
#include "../core/allocator.hpp"
#include "../core/memory.hpp"

namespace dsc {

// ── FenwickTree<T> (Binary Indexed Tree) ──────────────────────────────────────
// Compact structure for prefix-sum queries and point updates in O(log n).
// Uses ~n words of memory — the most space-efficient range-query structure.
//
// The key insight: each index i stores the sum of a[i - lowbit(i) + 1 .. i],
// where lowbit(i) = i & (-i).  This enables prefix queries and point updates
// in exactly ceil(log₂ n) iterations of +/- lowbit.
//
// Operations:
//   update(i, delta)    — add delta to position i (0-based)       O(log n)
//   prefix_sum(i)       — sum of [0, i] (inclusive, 0-based)      O(log n)
//   range_sum(l, r)     — sum of [l, r] (inclusive, 0-based)      O(log n)
//   point_query(i)      — current value at index i                 O(log n)
//   build(arr, n)       — build from array in O(n) (not O(n log n))
template<typename T>
class FenwickTree {
    T*    tree_;   // 1-indexed internally; tree_[0] unused
    usize n_;

public:
    explicit FenwickTree(usize n) : n_(n) {
        tree_ = Allocator<T>::allocate(n + 1);
        for (usize i = 0; i <= n; ++i) Allocator<T>::construct(tree_ + i, T{});
    }

    // Build from array in O(n) using the parent-propagation trick
    FenwickTree(const T* arr, usize n) : FenwickTree(n) {
        for (usize i = 0; i < n; ++i) {
            usize idx = i + 1;
            tree_[idx] += arr[i];
            usize parent = idx + (idx & (-idx));  // next responsible index
            if (parent <= n) tree_[parent] += tree_[idx];
        }
    }

    FenwickTree(const FenwickTree& o) : n_(o.n_) {
        tree_ = Allocator<T>::allocate(n_ + 1);
        for (usize i = 0; i <= n_; ++i) Allocator<T>::construct(tree_+i, o.tree_[i]);
    }

    FenwickTree(FenwickTree&& o) noexcept : tree_(o.tree_), n_(o.n_) {
        o.tree_ = nullptr; o.n_ = 0;
    }

    ~FenwickTree() {
        if (tree_) {
            for (usize i = 0; i <= n_; ++i) Allocator<T>::destroy(tree_+i);
            Allocator<T>::deallocate(tree_, n_+1);
        }
    }

    FenwickTree& operator=(const FenwickTree& o) {
        if (this != &o) { FenwickTree t(o); *this = traits::move(t); }
        return *this;
    }
    FenwickTree& operator=(FenwickTree&& o) noexcept {
        if (this != &o) {
            this->~FenwickTree();
            tree_ = o.tree_; n_ = o.n_;
            o.tree_ = nullptr; o.n_ = 0;
        }
        return *this;
    }

    // ── Point update ──────────────────────────────────────────────────────────
    // Add delta to element at 0-based index i. O(log n).
    void update(usize i, T delta) noexcept {
        for (usize j = i + 1; j <= n_; j += j & (-j))
            tree_[j] += delta;
    }

    // ── Prefix sum [0, i] (0-based inclusive) ─────────────────────────────────
    [[nodiscard]] T prefix_sum(usize i) const noexcept {
        T s{};
        for (usize j = i + 1; j > 0; j -= j & (-j))
            s += tree_[j];
        return s;
    }

    // ── Range sum [l, r] (0-based inclusive) ──────────────────────────────────
    [[nodiscard]] T range_sum(usize l, usize r) const noexcept {
        if (l == 0) return prefix_sum(r);
        return prefix_sum(r) - prefix_sum(l - 1);
    }

    // ── Point query (current value at index i) ────────────────────────────────
    [[nodiscard]] T point_query(usize i) const noexcept {
        if (i == 0) return prefix_sum(0);
        return prefix_sum(i) - prefix_sum(i - 1);
    }

    // ── Lower bound ───────────────────────────────────────────────────────────
    // Smallest index i such that prefix_sum(i) >= target. O(log n).
    // Only valid when all values are non-negative.
    [[nodiscard]] usize lower_bound(T target) const noexcept {
        usize idx = 0;
        usize step = usize(1) << detail_log2(n_);
        for (; step > 0; step >>= 1) {
            if (idx + step <= n_ && tree_[idx + step] < target) {
                idx += step;
                target -= tree_[idx];
            }
        }
        return idx;  // 0-based
    }

    [[nodiscard]] usize size() const noexcept { return n_; }

private:
    static usize detail_log2(usize n) noexcept {
        usize r = 0; while ((usize(1) << (r+1)) <= n) ++r; return r;
    }
};

// ── 2D Fenwick Tree ───────────────────────────────────────────────────────────
// O(log n * log m) point update and prefix-rect query on an n×m grid.
template<typename T>
class FenwickTree2D {
    T*    tree_;
    usize rows_, cols_;

    usize idx(usize r, usize c) const noexcept { return r * (cols_+1) + c; }

public:
    FenwickTree2D(usize rows, usize cols) : rows_(rows), cols_(cols) {
        tree_ = Allocator<T>::allocate((rows+1)*(cols+1));
        for (usize i = 0; i < (rows+1)*(cols+1); ++i) Allocator<T>::construct(tree_+i, T{});
    }

    ~FenwickTree2D() {
        if (tree_) {
            for (usize i = 0; i < (rows_+1)*(cols_+1); ++i) Allocator<T>::destroy(tree_+i);
            Allocator<T>::deallocate(tree_, (rows_+1)*(cols_+1));
        }
    }

    void update(usize r, usize c, T v) noexcept {
        for (usize i = r+1; i <= rows_; i += i & (-i))
            for (usize j = c+1; j <= cols_; j += j & (-j))
                tree_[idx(i,j)] += v;
    }

    [[nodiscard]] T prefix_sum(usize r, usize c) const noexcept {
        T s{};
        for (usize i = r+1; i > 0; i -= i & (-i))
            for (usize j = c+1; j > 0; j -= j & (-j))
                s += tree_[idx(i,j)];
        return s;
    }

    [[nodiscard]] T range_sum(usize r1, usize c1, usize r2, usize c2) const noexcept {
        T ans = prefix_sum(r2, c2);
        if (r1 > 0) ans -= prefix_sum(r1-1, c2);
        if (c1 > 0) ans -= prefix_sum(r2,   c1-1);
        if (r1 > 0 && c1 > 0) ans += prefix_sum(r1-1, c1-1);
        return ans;
    }
};

} // namespace dsc
