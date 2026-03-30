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

/// @brief Disjoint Set Union (DSU / Union-Find) data structure.
///
/// Tracks a partition of the integer range `[0, n)` into disjoint sets.
/// Two optimisations together achieve amortised O(α(n)) per operation, where
/// α is the inverse Ackermann function (≤ 5 for all practical inputs):
///
///  - **Path compression**: every `find()` call flattens the path to the root,
///    so subsequent `find()` calls on the same chain hit the root in O(1).
///  - **Union by rank**: always attaches the shorter tree under the taller,
///    bounding tree heights at O(log n).
///
/// Common applications: Kruskal's MST, connected-component labelling, cycle
/// detection, percolation theory, image segmentation.
class UnionFind {
    usize* parent_;
    usize* rank_;
    usize  n_;
    usize  components_;

public:
    /// @brief Constructs a DSU over `n` singleton elements `{0}, {1}, …, {n-1}`.
    /// @param n Universe size.
    /// @complexity O(n).
    explicit UnionFind(usize n) : n_(n), components_(n) {
        parent_ = mem::alloc_n<usize>(n);
        rank_   = mem::alloc_n<usize>(n);
        for (usize i = 0; i < n; ++i) { parent_[i] = i; rank_[i] = 0; }
    }

    /// @brief Copy constructor. Deep-copies parent and rank arrays.
    /// @complexity O(n).
    UnionFind(const UnionFind& o) : n_(o.n_), components_(o.components_) {
        parent_ = mem::alloc_n<usize>(n_);
        rank_   = mem::alloc_n<usize>(n_);
        mem::copy(parent_, o.parent_, n_ * sizeof(usize));
        mem::copy(rank_,   o.rank_,   n_ * sizeof(usize));
    }

    /// @brief Move constructor. Transfers ownership from @p o.
    /// @param o Source DSU. Left in a valid but empty state.
    /// @complexity O(1).
    UnionFind(UnionFind&& o) noexcept
        : parent_(o.parent_), rank_(o.rank_), n_(o.n_), components_(o.components_)
    {
        o.parent_ = o.rank_ = nullptr; o.n_ = o.components_ = 0;
    }

    /// @brief Destructor. Frees both internal arrays.
    /// @complexity O(1).
    ~UnionFind() {
        mem::dealloc_n(parent_);
        mem::dealloc_n(rank_);
    }

    /// @brief Copy-assignment operator.
    /// @return Reference to `*this`.
    /// @complexity O(n).
    UnionFind& operator=(const UnionFind& o) {
        if (this != &o) { UnionFind t(o); *this = traits::move(t); }
        return *this;
    }

    /// @brief Move-assignment operator.
    /// @return Reference to `*this`.
    /// @complexity O(1).
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

    /// @brief Returns the representative (root) of the set containing @p x.
    ///
    /// Uses iterative path compression: all nodes on the path from @p x to the
    /// root are re-pointed directly at the root, flattening the tree for future
    /// queries. Iterative (not recursive) to avoid stack overflow on deep trees
    /// before their first compression.
    ///
    /// @param x Element index. Must be in `[0, size())`.
    /// @return Root of the component containing @p x.
    /// @complexity O(α(n)) amortised.
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

    /// @brief Merges the sets containing @p x and @p y.
    ///
    /// The shorter tree (by rank) is attached under the taller one. If both
    /// trees have equal rank, one is chosen arbitrarily and its root's rank
    /// is incremented.
    ///
    /// @param x First element.
    /// @param y Second element.
    /// @return `true` if @p x and @p y were in different sets (union performed);
    ///         `false` if they were already in the same set.
    /// @complexity O(α(n)) amortised.
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

    /// @brief Returns `true` if @p x and @p y belong to the same set.
    /// @complexity O(α(n)) amortised.
    [[nodiscard]] bool connected(usize x, usize y) noexcept {
        return find(x) == find(y);
    }

    /// @brief Returns the current number of disjoint components.
    /// @complexity O(1).
    [[nodiscard]] usize component_count() const noexcept { return components_; }

    /// @brief Returns the universe size (number of elements).
    /// @complexity O(1).
    [[nodiscard]] usize size()            const noexcept { return n_; }

    /// @brief Resets all elements to singleton sets, discarding all unions.
    /// @complexity O(n).
    void reset() noexcept {
        for (usize i = 0; i < n_; ++i) { parent_[i] = i; rank_[i] = 0; }
        components_ = n_;
    }
};

} // namespace dsc
