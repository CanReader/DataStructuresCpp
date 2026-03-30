#pragma once

#include "../core/primitives.hpp"
#include "../core/allocator.hpp"
#include "../core/memory.hpp"

namespace dsc {

// ── SparseSet<U> ──────────────────────────────────────────────────────────────
// Ultra-fast set for small unsigned integers in [0, U).
//
// Two arrays:
//   dense_[0..size_) — packed list of elements in insertion order.
//   sparse_[0..U)    — sparse_[x] = index of x in dense_ (only valid if x is present).
//
// Because we only check sparse_[x] when we can verify dense_[sparse_[x]] == x,
// sparse_ needs no initialisation — it can contain garbage.
//
// Complexity:
//   insert   : O(1)
//   erase    : O(1) (swap with last in dense, update sparse)
//   contains : O(1)
//   clear    : O(1) (just set size_ = 0)
//   iteration: O(n) over dense_ — cache-perfectly-sequential
//
// Limitation: elements must be < UNIVERSE (set at compile time or construction).
/// @brief Ultra-fast set for small unsigned integers using sparse-dense representation.
/// @tparam T Element type (must be unsigned integral).
template<typename T = usize>
class SparseSet {
    static_assert(traits::is_unsigned_v<T> || traits::is_integral_v<T>);

    T*    dense_;
    usize* sparse_;
    usize  size_;
    usize  universe_;

public:
    /// @brief Constructs a sparse set for elements in [0, universe).
    /// @param universe The maximum value + 1 (elements must be < universe).
    /// @pre universe > 0
    explicit SparseSet(usize universe)
        : size_(0), universe_(universe)
    {
        dense_  = Allocator<T>::allocate(universe);
        sparse_ = Allocator<usize>::allocate(universe);
        // sparse_ intentionally NOT initialised — garbage is fine
    }

    /// @brief Copy constructor.
    /// @param o The SparseSet to copy from.
    SparseSet(const SparseSet& o) : size_(o.size_), universe_(o.universe_) {
        dense_  = Allocator<T>::allocate(universe_);
        sparse_ = Allocator<usize>::allocate(universe_);
        mem::copy(dense_,  o.dense_,  size_     * sizeof(T));
        mem::copy(sparse_, o.sparse_, universe_ * sizeof(usize));
    }

    /// @brief Move constructor.
    /// @param o The SparseSet to move from.
    SparseSet(SparseSet&& o) noexcept
        : dense_(o.dense_), sparse_(o.sparse_), size_(o.size_), universe_(o.universe_)
    {
        o.dense_ = nullptr; o.sparse_ = nullptr; o.size_ = 0;
    }

    /// @brief Destructor.
    ~SparseSet() {
        Allocator<T>::deallocate(dense_,  universe_);
        Allocator<usize>::deallocate(sparse_, universe_);
    }

    /// @brief Copy assignment operator.
    /// @param o The SparseSet to copy from.
    /// @return Reference to this SparseSet.
    SparseSet& operator=(const SparseSet& o) {
        if (this != &o) { SparseSet t(o); *this = traits::move(t); }
        return *this;
    }
    /// @brief Move assignment operator.
    /// @param o The SparseSet to move from.
    /// @return Reference to this SparseSet.
    SparseSet& operator=(SparseSet&& o) noexcept {
        if (this != &o) {
            this->~SparseSet();
            dense_=o.dense_; sparse_=o.sparse_; size_=o.size_; universe_=o.universe_;
            o.dense_=nullptr; o.sparse_=nullptr;
        }
        return *this;
    }

    // ── Insert ────────────────────────────────────────────────────────────────
    /// @brief Inserts an element into the set.
    /// @param x The element to insert (must be < universe).
    /// @complexity O(1)
    void insert(T x) noexcept {
        usize ux = static_cast<usize>(x);
        if (ux >= universe_ || contains(x)) return;
        dense_[size_]  = x;
        sparse_[ux]    = size_;
        ++size_;
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    /// @brief Removes an element from the set.
    /// @param x The element to remove.
    /// @complexity O(1)
    void erase(T x) noexcept {
        if (!contains(x)) return;
        usize ux   = static_cast<usize>(x);
        usize idx  = sparse_[ux];
        T     last = dense_[size_ - 1];
        dense_[idx]                   = last;
        sparse_[static_cast<usize>(last)] = idx;
        --size_;
    }

    // ── Lookup ────────────────────────────────────────────────────────────────
    /// @brief Checks if an element is in the set.
    /// @param x The element to check.
    /// @return true if present, false otherwise.
    /// @complexity O(1)
    [[nodiscard]] bool contains(T x) const noexcept {
        usize ux = static_cast<usize>(x);
        return ux < universe_ && sparse_[ux] < size_ && dense_[sparse_[ux]] == x;
    }

    // ── State ─────────────────────────────────────────────────────────────────
    /// @brief Returns the number of elements in the set.
    /// @return The size.
    [[nodiscard]] usize size()     const noexcept { return size_; }
    /// @brief Checks if the set is empty.
    /// @return true if empty, false otherwise.
    [[nodiscard]] bool  empty()    const noexcept { return size_ == 0; }
    /// @brief Returns the universe size.
    /// @return The universe.
    [[nodiscard]] usize universe() const noexcept { return universe_; }

    /// @brief Clears the set.
    /// @complexity O(1)
    void clear() noexcept { size_ = 0; }  // O(1) — no init needed

    // ── Iteration (cache-sequential over dense_) ──────────────────────────────
    /// @brief Returns a const iterator to the beginning.
    /// @return Const iterator.
    const T* begin() const noexcept { return dense_; }
    /// @brief Returns a const iterator to the end.
    /// @return Const iterator.
    const T* end()   const noexcept { return dense_ + size_; }
    /// @brief Returns an iterator to the beginning.
    /// @return Iterator.
    T*       begin()       noexcept { return dense_; }
    /// @brief Returns an iterator to the end.
    /// @return Iterator.
    T*       end()         noexcept { return dense_ + size_; }
};

} // namespace dsc
