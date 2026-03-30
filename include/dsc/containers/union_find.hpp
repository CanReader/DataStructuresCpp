#pragma once

#include "../core/primitives.hpp"
#include "../core/allocator.hpp"
#include "../core/memory.hpp"

namespace dsc {

// ── UnionFind (Disjoint Set Union) ────────────────────────────────────────────
// Tracks a partition of [0, n) into disjoint sets.
//
// Two optimisations for near-O(1) operations:
//   • Path compression  — flatten the tree on every find(), so future
//                         find() calls on the same chain are O(1).
//   • Union by rank     — always attach the shorter tree under the taller,
//                         keeping tree heights bounded at O(log n).
//
// Combined: amortised O(α(n)) per operation (α = inverse Ackermann ≤ 5 always).
//
// Applications: Kruskal's MST, connected components, cycle detection,
//               percolation, image segmentation.
class UnionFind {
    usize* parent_;
    usize* rank_;
    usize  n_;
    usize  components_;

public:
    explicit UnionFind(usize n) : n_(n), components_(n) {
        parent_ = mem::alloc_n<usize>(n);
        rank_   = mem::alloc_n<usize>(n);
        for (usize i = 0; i < n; ++i) { parent_[i] = i; rank_[i] = 0; }
    }

    UnionFind(const UnionFind& o) : n_(o.n_), components_(o.components_) {
        parent_ = mem::alloc_n<usize>(n_);
        rank_   = mem::alloc_n<usize>(n_);
        mem::copy(parent_, o.parent_, n_ * sizeof(usize));
        mem::copy(rank_,   o.rank_,   n_ * sizeof(usize));
    }

    UnionFind(UnionFind&& o) noexcept
        : parent_(o.parent_), rank_(o.rank_), n_(o.n_), components_(o.components_)
    {
        o.parent_ = o.rank_ = nullptr; o.n_ = o.components_ = 0;
    }

    ~UnionFind() {
        mem::dealloc_n(parent_);
        mem::dealloc_n(rank_);
    }

    UnionFind& operator=(const UnionFind& o) {
        if (this != &o) { UnionFind t(o); *this = traits::move(t); }
        return *this;
    }

    UnionFind& operator=(UnionFind&& o) noexcept {
        if (this != &o) {
            mem::dealloc_n(parent_); mem::dealloc_n(rank_);
            parent_ = o.parent_; rank_ = o.rank_;
            n_ = o.n_; components_ = o.components_;
            o.parent_ = o.rank_ = nullptr;
        }
        return *this;
    }

    // ── Find with path compression ────────────────────────────────────────────
    // Iterative (avoids stack overflow on deep trees before compression).
    [[nodiscard]] usize find(usize x) noexcept {
        // Find root
        usize root = x;
        while (parent_[root] != root) root = parent_[root];
        // Compress: point all nodes on the path directly to root
        while (parent_[x] != root) {
            usize next = parent_[x];
            parent_[x] = root;
            x = next;
        }
        return root;
    }

    // ── Union by rank ─────────────────────────────────────────────────────────
    // Returns true if x and y were in different sets (union performed).
    bool unite(usize x, usize y) noexcept {
        usize rx = find(x), ry = find(y);
        if (rx == ry) return false;
        if (rank_[rx] < rank_[ry]) { usize t = rx; rx = ry; ry = t; }
        parent_[ry] = rx;
        if (rank_[rx] == rank_[ry]) ++rank_[rx];
        --components_;
        return true;
    }

    // ── Queries ───────────────────────────────────────────────────────────────
    [[nodiscard]] bool connected(usize x, usize y) noexcept {
        return find(x) == find(y);
    }

    [[nodiscard]] usize component_count() const noexcept { return components_; }
    [[nodiscard]] usize size()            const noexcept { return n_; }

    // Reset to n isolated elements
    void reset() noexcept {
        for (usize i = 0; i < n_; ++i) { parent_[i] = i; rank_[i] = 0; }
        components_ = n_;
    }
};

} // namespace dsc
