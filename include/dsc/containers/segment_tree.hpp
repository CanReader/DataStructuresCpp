#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"

namespace dsc {

// ── SegmentTree<T, Combine> ───────────────────────────────────────────────────
// Implicit binary tree stored in a flat array (1-indexed, classic layout).
// Supports point updates and range queries in O(log n).
//
// The Combine functor defines the monoid operation (sum, min, max, gcd, …).
// The identity element `IDENTITY` must be the neutral element for Combine.
//
// Layout: node 1 = root; children of i = 2i, 2i+1.
// Internal array size = 4*n (safe over-allocation, avoids exact pow2 calc).
//
// Example: range sum
//   SegmentTree<int, decltype([](int a, int b){ return a+b; })> seg(arr, n, 0);
//
// Example: range min
//   SegmentTree<int, decltype([](int a, int b){ return a<b?a:b; })> seg(arr, n, INT_MAX);

/// @brief Implicit segment tree supporting point updates and range queries in O(log n).
///
/// Nodes are stored in a flat 1-indexed array of size `4 × n`. The root is at
/// index 1; for any internal node at index `i`, its children are at `2i` and
/// `2i+1`.
///
/// The tree is parameterised by a **Combine** functor that defines the monoid
/// operation (sum, min, max, GCD, …). An **identity** value (neutral element for
/// Combine) must be supplied at construction.
///
/// @tparam T       Element type and query return type.
/// @tparam Combine Binary functor: `combine(a, b)` must be associative with
///                 the supplied identity. Defaults to addition.
///
/// @par Example — range sum:
/// @code
/// dsc::SegmentTree<int, decltype([](int a, int b){ return a+b; })> seg(arr, n, 0);
/// seg.update(2, 10);      // set index 2 to 10
/// int s = seg.query(1, 4); // sum of [1, 4]
/// @endcode
template<typename T,
         typename Combine = decltype([](const T& a, const T& b){ return a + b; })>
class SegmentTree {
    T*      tree_;
    usize   n_;
    T       identity_;
    Combine combine_{};

    void build(const T* arr, usize node, usize lo, usize hi) noexcept {
        if (lo == hi) { tree_[node] = arr[lo]; return; }
        usize mid = (lo + hi) / 2;
        build(arr, 2*node,   lo,    mid);
        build(arr, 2*node+1, mid+1, hi);
        tree_[node] = combine_(tree_[2*node], tree_[2*node+1]);
    }

    void update(usize node, usize lo, usize hi, usize i, const T& v) noexcept {
        if (lo == hi) { tree_[node] = v; return; }
        usize mid = (lo + hi) / 2;
        if (i <= mid) update(2*node,   lo,    mid, i, v);
        else          update(2*node+1, mid+1, hi,  i, v);
        tree_[node] = combine_(tree_[2*node], tree_[2*node+1]);
    }

    T query(usize node, usize lo, usize hi, usize l, usize r) const noexcept {
        if (r < lo || hi < l) return identity_;
        if (l <= lo && hi <= r) return tree_[node];
        usize mid = (lo + hi) / 2;
        return combine_(
            query(2*node,   lo,    mid, l, r),
            query(2*node+1, mid+1, hi,  l, r)
        );
    }

public:
    /// @brief Constructs the segment tree from an existing array.
    ///
    /// Allocates an internal array of size `4 × n` and builds the tree in O(n).
    /// @param arr      Source array of @p n elements.
    /// @param n        Number of elements.
    /// @param identity Neutral element for @p Combine (e.g. 0 for sum, INT_MAX for min).
    /// @complexity O(n).
    SegmentTree(const T* arr, usize n, T identity)
        : n_(n), identity_(traits::move(identity))
    {
        tree_ = Allocator<T>::allocate(4 * n);
        for (usize i = 0; i < 4*n; ++i) Allocator<T>::construct(tree_ + i, identity_);
        if (n > 0) build(arr, 1, 0, n - 1);
    }

    /// @brief Constructs a segment tree of @p n elements all initialised to @p identity.
    /// @param n        Number of elements.
    /// @param identity Value to fill all positions with; also the neutral element.
    /// @complexity O(n).
    SegmentTree(usize n, T identity)
        : n_(n), identity_(identity)
    {
        tree_ = Allocator<T>::allocate(4 * n);
        for (usize i = 0; i < 4*n; ++i) Allocator<T>::construct(tree_ + i, identity_);
    }

    /// @brief Move constructor. Transfers ownership of the internal array.
    /// @complexity O(1).
    SegmentTree(SegmentTree&& o) noexcept
        : tree_(o.tree_), n_(o.n_), identity_(traits::move(o.identity_))
    {
        o.tree_ = nullptr; o.n_ = 0;
    }

    /// @brief Destructor. Destroys all tree nodes and releases the allocation.
    /// @complexity O(n).
    ~SegmentTree() {
        if (tree_) {
            for (usize i = 0; i < 4*n_; ++i) Allocator<T>::destroy(tree_ + i);
            Allocator<T>::deallocate(tree_, 4*n_);
        }
    }

    // ── Point update ──────────────────────────────────────────────────────────

    /// @brief Sets the element at 0-based index @p i to @p v.
    ///
    /// Walks down the tree to the leaf at @p i, sets its value, then
    /// propagates the combined result back up to the root.
    /// @param i 0-based index. Must be in `[0, size())`.
    /// @param v New value to assign.
    /// @complexity O(log n).
    void update(usize i, const T& v) noexcept { update(1, 0, n_-1, i, v); }

    // ── Range query ───────────────────────────────────────────────────────────

    /// @brief Returns the combined value over the inclusive range `[l, r]`.
    ///
    /// Out-of-range sub-queries return the identity value and are silently
    /// discarded by the recursion.
    /// @param l Left bound (0-based, inclusive).
    /// @param r Right bound (0-based, inclusive). Must satisfy `l <= r < size()`.
    /// @return `Combine`-reduced value for all elements in `[l, r]`.
    /// @complexity O(log n).
    [[nodiscard]] T query(usize l, usize r) const noexcept {
        return query(1, 0, n_-1, l, r);
    }

    /// @brief Returns the number of elements in the segment tree.
    /// @complexity O(1).
    [[nodiscard]] usize size() const noexcept { return n_; }
};

// ── LazySegmentTree<T, Combine, Apply> ───────────────────────────────────────
// Segment tree with lazy propagation for range updates + range queries.
// Supports: range assign/add, then range min/max/sum query — all O(log n).
//
// The Lazy type must have a "no-op" identity that Apply(identity, node) = node.

/// @brief Segment tree with lazy propagation for O(log n) range updates and queries.
///
/// Extends `SegmentTree` with a lazy tag array. Pending updates are stored
/// at internal nodes and pushed down to children only when needed (`push_down`).
/// This makes range-update + range-query workloads O(log n) per operation.
///
/// @tparam T       Element type.
/// @tparam Lazy    Pending-update (lazy) type. Must support `==` for identity
///                 comparison and `+` for composing two pending updates.
/// @tparam Combine Binary monoid functor (e.g. addition). Defaults to addition.
/// @tparam Apply   Functor `apply(node_val, lazy_val, segment_len)` that applies
///                 a lazy update to a node value. Defaults to additive range add.
///
/// @par Example — range add, range sum:
/// @code
/// // identity for sum = 0; identity for lazy = 0
/// dsc::LazySegmentTree<int, int> seg(arr, n, 0, 0);
/// seg.update(1, 3, 5);       // add 5 to elements [1, 3]
/// int s = seg.query(0, 4);   // sum of [0, 4]
/// @endcode
template<typename T, typename Lazy,
         typename Combine = decltype([](const T& a, const T& b){ return a + b; }),
         typename Apply   = decltype([](const T& node, const Lazy& lazy, usize len){ return node + lazy * static_cast<T>(len); })>
class LazySegmentTree {
    T*     tree_;
    Lazy*  lazy_;
    usize  n_;
    T      t_identity_;
    Lazy   l_identity_;
    Combine combine_{};
    Apply   apply_{};

    void push_down(usize node, usize lo, usize hi) noexcept {
        if (lazy_[node] == l_identity_) return;
        usize mid = (lo + hi) / 2;
        usize llen = mid - lo + 1, rlen = hi - mid;
        tree_[2*node]   = apply_(tree_[2*node],   lazy_[node], llen);
        tree_[2*node+1] = apply_(tree_[2*node+1], lazy_[node], rlen);
        lazy_[2*node]   = lazy_[2*node]   == l_identity_ ? lazy_[node] : lazy_[2*node] + lazy_[node];
        lazy_[2*node+1] = lazy_[2*node+1] == l_identity_ ? lazy_[node] : lazy_[2*node+1] + lazy_[node];
        lazy_[node] = l_identity_;
    }

    void build(const T* arr, usize node, usize lo, usize hi) {
        lazy_[node] = l_identity_;
        if (lo == hi) { tree_[node] = arr[lo]; return; }
        usize mid = (lo+hi)/2;
        build(arr, 2*node, lo, mid);
        build(arr, 2*node+1, mid+1, hi);
        tree_[node] = combine_(tree_[2*node], tree_[2*node+1]);
    }

    void range_update(usize node, usize lo, usize hi, usize l, usize r, const Lazy& v) noexcept {
        if (r < lo || hi < l) return;
        if (l <= lo && hi <= r) {
            tree_[node] = apply_(tree_[node], v, hi-lo+1);
            lazy_[node] = lazy_[node] == l_identity_ ? v : lazy_[node] + v;
            return;
        }
        push_down(node, lo, hi);
        usize mid = (lo+hi)/2;
        range_update(2*node,   lo,    mid, l, r, v);
        range_update(2*node+1, mid+1, hi,  l, r, v);
        tree_[node] = combine_(tree_[2*node], tree_[2*node+1]);
    }

    T range_query(usize node, usize lo, usize hi, usize l, usize r) noexcept {
        if (r < lo || hi < l) return t_identity_;
        if (l <= lo && hi <= r) return tree_[node];
        push_down(node, lo, hi);
        usize mid = (lo+hi)/2;
        return combine_(
            range_query(2*node,   lo,    mid, l, r),
            range_query(2*node+1, mid+1, hi,  l, r)
        );
    }

public:
    /// @brief Constructs a lazy segment tree from an existing array.
    ///
    /// @param arr    Source array of @p n elements.
    /// @param n      Number of elements.
    /// @param t_id   Identity element for @p Combine (e.g. 0 for sum).
    /// @param l_id   Identity element for @p Lazy (no-op lazy tag, e.g. 0 for additive lazy).
    /// @complexity O(n).
    LazySegmentTree(const T* arr, usize n, T t_id, Lazy l_id)
        : n_(n), t_identity_(t_id), l_identity_(l_id)
    {
        tree_ = Allocator<T>::allocate(4*n);
        lazy_ = Allocator<Lazy>::allocate(4*n);
        for (usize i = 0; i < 4*n; ++i) {
            Allocator<T>::construct(tree_+i, t_id);
            Allocator<Lazy>::construct(lazy_+i, l_id);
        }
        if (n > 0) build(arr, 1, 0, n-1);
    }

    /// @brief Destructor. Destroys all nodes and releases both arrays.
    /// @complexity O(n).
    ~LazySegmentTree() {
        if (tree_) {
            for (usize i = 0; i < 4*n_; ++i) {
                Allocator<T>::destroy(tree_+i);
                Allocator<Lazy>::destroy(lazy_+i);
            }
            Allocator<T>::deallocate(tree_, 4*n_);
            Allocator<Lazy>::deallocate(lazy_, 4*n_);
        }
    }

    /// @brief Applies the lazy value @p v to all elements in the inclusive range `[l, r]`.
    ///
    /// The update is deferred — internal nodes store the pending tag until it
    /// is pushed down by a subsequent query or overlapping update.
    /// @param l Left bound (0-based, inclusive).
    /// @param r Right bound (0-based, inclusive).
    /// @param v Lazy update value to apply.
    /// @complexity O(log n).
    void update(usize l, usize r, const Lazy& v) noexcept { range_update(1, 0, n_-1, l, r, v); }

    /// @brief Returns the combined value over the inclusive range `[l, r]`.
    /// @param l Left bound (0-based, inclusive).
    /// @param r Right bound (0-based, inclusive).
    /// @complexity O(log n).
    [[nodiscard]] T query(usize l, usize r) noexcept { return range_query(1, 0, n_-1, l, r); }

    /// @brief Returns the number of elements in the tree.
    /// @complexity O(1).
    [[nodiscard]] usize size() const noexcept { return n_; }
};

} // namespace dsc
