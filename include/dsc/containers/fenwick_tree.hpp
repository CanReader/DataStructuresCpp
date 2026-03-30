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
/// @brief Binary Indexed Tree (Fenwick Tree) for efficient prefix sum queries and point updates.
/// @tparam T Element type (must support addition).
template<typename T>
class FenwickTree {
    T*    tree_;   // 1-indexed internally; tree_[0] unused
    usize n_;

public:
    /// @brief Constructs a Fenwick Tree with the specified size.
    /// @param n The number of elements (0-based indexing up to n-1).
    /// @pre n > 0
    explicit FenwickTree(usize n) : n_(n) {
        tree_ = Allocator<T>::allocate(n + 1);
        for (usize i = 0; i <= n; ++i) Allocator<T>::construct(tree_ + i, T{});
    }

    /// @brief Constructs a Fenwick Tree from an array in O(n) time.
    /// @param arr Pointer to the array to build from.
    /// @param n Size of the array.
    /// @pre arr != nullptr and n > 0
    FenwickTree(const T* arr, usize n) : FenwickTree(n) {
        for (usize i = 0; i < n; ++i) {
            usize idx = i + 1;
            tree_[idx] += arr[i];
            usize parent = idx + (idx & (-idx));  // next responsible index
            if (parent <= n) tree_[parent] += tree_[idx];
        }
    }

    /// @brief Copy constructor.
    /// @param o The FenwickTree to copy from.
    FenwickTree(const FenwickTree& o) : n_(o.n_) {
        tree_ = Allocator<T>::allocate(n_ + 1);
        for (usize i = 0; i <= n_; ++i) Allocator<T>::construct(tree_+i, o.tree_[i]);
    }

    /// @brief Move constructor.
    /// @param o The FenwickTree to move from.
    FenwickTree(FenwickTree&& o) noexcept : tree_(o.tree_), n_(o.n_) {
        o.tree_ = nullptr; o.n_ = 0;
    }

    /// @brief Destructor.
    ~FenwickTree() {
        if (tree_) {
            for (usize i = 0; i <= n_; ++i) Allocator<T>::destroy(tree_+i);
            Allocator<T>::deallocate(tree_, n_+1);
        }
    }

    /// @brief Copy assignment operator.
    /// @param o The FenwickTree to copy from.
    /// @return Reference to this FenwickTree.
    FenwickTree& operator=(const FenwickTree& o) {
        if (this != &o) { FenwickTree t(o); *this = traits::move(t); }
        return *this;
    }
    /// @brief Move assignment operator.
    /// @param o The FenwickTree to move from.
    /// @return Reference to this FenwickTree.
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
    /// @brief Adds a delta value to the element at the specified index.
    /// @param i 0-based index of the element to update.
    /// @param delta Value to add to the element.
    /// @pre i < n_
    /// @complexity O(log n)
    void update(usize i, T delta) noexcept {
        for (usize j = i + 1; j <= n_; j += j & (-j))
            tree_[j] += delta;
    }

    // ── Prefix sum [0, i] (0-based inclusive) ─────────────────────────────────
    /// @brief Computes the prefix sum from index 0 to i (inclusive).
    /// @param i 0-based index up to which to compute the sum.
    /// @return The sum of elements from 0 to i.
    /// @pre i < n_
    /// @complexity O(log n)
    [[nodiscard]] T prefix_sum(usize i) const noexcept {
        T s{};
        for (usize j = i + 1; j > 0; j -= j & (-j))
            s += tree_[j];
        return s;
    }

    // ── Range sum [l, r] (0-based inclusive) ──────────────────────────────────
    /// @brief Computes the sum of elements from index l to r (inclusive).
    /// @param l 0-based start index.
    /// @param r 0-based end index.
    /// @return The sum of elements from l to r.
    /// @pre l <= r and r < n_
    /// @complexity O(log n)
    [[nodiscard]] T range_sum(usize l, usize r) const noexcept {
        if (l == 0) return prefix_sum(r);
        return prefix_sum(r) - prefix_sum(l - 1);
    }

    // ── Point query (current value at index i) ────────────────────────────────
    /// @brief Retrieves the current value at the specified index.
    /// @param i 0-based index of the element.
    /// @return The value at index i.
    /// @pre i < n_
    /// @complexity O(log n)
    [[nodiscard]] T point_query(usize i) const noexcept {
        if (i == 0) return prefix_sum(0);
        return prefix_sum(i) - prefix_sum(i - 1);
    }

    // ── Lower bound ───────────────────────────────────────────────────────────
    // Smallest index i such that prefix_sum(i) >= target. O(log n).
    // Only valid when all values are non-negative.
    /// @brief Finds the smallest index i such that the prefix sum up to i is at least the target.
    /// @param target The target sum value.
    /// @return The smallest 0-based index i where prefix_sum(i) >= target.
    /// @pre All values in the tree are non-negative.
    /// @complexity O(log n)
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

    /// @brief Returns the size of the Fenwick Tree.
    /// @return The number of elements.
    [[nodiscard]] usize size() const noexcept { return n_; }

private:
    static usize detail_log2(usize n) noexcept {
        usize r = 0; while ((usize(1) << (r+1)) <= n) ++r; return r;
    }
};

// ── 2D Fenwick Tree ───────────────────────────────────────────────────────────
// O(log n * log m) point update and prefix-rect query on an n×m grid.
/// @brief 2D Binary Indexed Tree (Fenwick Tree) for efficient prefix sum queries and point updates on a 2D grid.
/// @tparam T Element type (must support addition).
template<typename T>
class FenwickTree2D {
    T*    tree_;
    usize rows_, cols_;

    usize idx(usize r, usize c) const noexcept { return r * (cols_+1) + c; }

public:
    /// @brief Constructs a 2D Fenwick Tree with the specified dimensions.
    /// @param rows Number of rows.
    /// @param cols Number of columns.
    /// @pre rows > 0 and cols > 0
    FenwickTree2D(usize rows, usize cols) : rows_(rows), cols_(cols) {
        tree_ = Allocator<T>::allocate((rows+1)*(cols+1));
        for (usize i = 0; i < (rows+1)*(cols+1); ++i) Allocator<T>::construct(tree_+i, T{});
    }

    /// @brief Destructor.
    ~FenwickTree2D() {
        if (tree_) {
            for (usize i = 0; i < (rows_+1)*(cols_+1); ++i) Allocator<T>::destroy(tree_+i);
            Allocator<T>::deallocate(tree_, (rows_+1)*(cols_+1));
        }
    }

    /// @brief Adds a value to the element at the specified row and column.
    /// @param r 0-based row index.
    /// @param c 0-based column index.
    /// @param v Value to add.
    /// @pre r < rows_ and c < cols_
    /// @complexity O(log rows * log cols)
    void update(usize r, usize c, T v) noexcept {
        for (usize i = r+1; i <= rows_; i += i & (-i))
            for (usize j = c+1; j <= cols_; j += j & (-j))
                tree_[idx(i,j)] += v;
    }

    /// @brief Computes the prefix sum from (0,0) to (r,c) (inclusive).
    /// @param r 0-based row index.
    /// @param c 0-based column index.
    /// @return The sum of elements from (0,0) to (r,c).
    /// @pre r < rows_ and c < cols_
    /// @complexity O(log rows * log cols)
    [[nodiscard]] T prefix_sum(usize r, usize c) const noexcept {
        T s{};
        for (usize i = r+1; i > 0; i -= i & (-i))
            for (usize j = c+1; j > 0; j -= j & (-j))
                s += tree_[idx(i,j)];
        return s;
    }

    /// @brief Computes the sum of elements in the rectangle from (r1,c1) to (r2,c2) (inclusive).
    /// @param r1 0-based start row index.
    /// @param c1 0-based start column index.
    /// @param r2 0-based end row index.
    /// @param c2 0-based end column index.
    /// @return The sum of elements in the specified rectangle.
    /// @pre r1 <= r2 < rows_ and c1 <= c2 < cols_
    /// @complexity O(log rows * log cols)
    [[nodiscard]] T range_sum(usize r1, usize c1, usize r2, usize c2) const noexcept {
        T ans = prefix_sum(r2, c2);
        if (r1 > 0) ans -= prefix_sum(r1-1, c2);
        if (c1 > 0) ans -= prefix_sum(r2,   c1-1);
        if (r1 > 0 && c1 > 0) ans += prefix_sum(r1-1, c1-1);
        return ans;
    }
};

} // namespace dsc
