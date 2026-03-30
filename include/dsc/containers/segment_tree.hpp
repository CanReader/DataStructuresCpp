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
    // Build from array [arr, arr+n)
    SegmentTree(const T* arr, usize n, T identity)
        : n_(n), identity_(traits::move(identity))
    {
        tree_ = Allocator<T>::allocate(4 * n);
        for (usize i = 0; i < 4*n; ++i) Allocator<T>::construct(tree_ + i, identity_);
        if (n > 0) build(arr, 1, 0, n - 1);
    }

    // Build with all elements equal to identity
    SegmentTree(usize n, T identity)
        : n_(n), identity_(identity)
    {
        tree_ = Allocator<T>::allocate(4 * n);
        for (usize i = 0; i < 4*n; ++i) Allocator<T>::construct(tree_ + i, identity_);
    }

    SegmentTree(SegmentTree&& o) noexcept
        : tree_(o.tree_), n_(o.n_), identity_(traits::move(o.identity_))
    {
        o.tree_ = nullptr; o.n_ = 0;
    }

    ~SegmentTree() {
        if (tree_) {
            for (usize i = 0; i < 4*n_; ++i) Allocator<T>::destroy(tree_ + i);
            Allocator<T>::deallocate(tree_, 4*n_);
        }
    }

    // ── Point update ──────────────────────────────────────────────────────────
    // Set element at index i (0-based) to value v. O(log n).
    void update(usize i, const T& v) noexcept { update(1, 0, n_-1, i, v); }

    // ── Range query ───────────────────────────────────────────────────────────
    // Query combined value over [l, r] (0-based, inclusive). O(log n).
    [[nodiscard]] T query(usize l, usize r) const noexcept {
        return query(1, 0, n_-1, l, r);
    }

    [[nodiscard]] usize size() const noexcept { return n_; }
};

// ── LazySegmentTree<T, Combine, Apply> ───────────────────────────────────────
// Segment tree with lazy propagation for range updates + range queries.
// Supports: range assign/add, then range min/max/sum query — all O(log n).
//
// The Lazy type must have a "no-op" identity that Apply(identity, node) = node.
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

    // Range update: apply lazy value v to all elements in [l, r]. O(log n).
    void update(usize l, usize r, const Lazy& v) noexcept { range_update(1, 0, n_-1, l, r, v); }

    // Range query over [l, r]. O(log n).
    [[nodiscard]] T query(usize l, usize r) noexcept { return range_query(1, 0, n_-1, l, r); }

    [[nodiscard]] usize size() const noexcept { return n_; }
};

} // namespace dsc
