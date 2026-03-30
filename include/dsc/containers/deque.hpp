#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/memory.hpp"
#include "../core/allocator.hpp"

namespace dsc {

// ── Deque<T> ──────────────────────────────────────────────────────────────────
// Double-ended queue using a segmented (block-map) design.
//
// Memory layout:
//   • Elements are stored in fixed-size blocks of BLOCK elements each.
//   • A central "map" array holds pointers to those blocks.
//   • Both ends can grow by adding new blocks without moving existing data.
//   • O(1) amortized push/pop at both ends; O(1) random access.
//   • Far better than std::deque's typical node overhead because block size
//     is compile-time constant and the map is a simple pointer array.

/// @brief Double-ended queue using a block-segmented (map) memory layout.
///
/// Elements are stored in fixed-size contiguous blocks of `BLOCK` elements.
/// A central map array holds pointers to those blocks, enabling O(1) amortised
/// push and pop at both ends without relocating existing elements. Random
/// access is O(1) via two-level indexing (map index + intra-block offset).
///
/// The map itself is allocated with extra headroom at both ends so that the
/// common case of alternating front/back pushes avoids map reallocation.
///
/// @tparam T     Element type. Must be constructible / destructible.
/// @tparam BLOCK Number of elements per block. Must be >= 4. Larger values
///               improve cache locality; smaller values reduce wasted space at
///               the ends. Defaults to 64.
template<typename T, usize BLOCK = 64>
class Deque {
    static_assert(BLOCK >= 4, "Block size must be at least 4");

    T**   map_;           // array of block pointers
    usize map_cap_;       // number of slots in map_
    usize map_begin_;     // index of first block in use
    usize map_end_;       // one past last block in use
    usize front_offset_;  // offset within first block of first element
    usize back_offset_;   // one past last element in last block
    usize size_;

    static constexpr usize MIN_MAP = 8;

public:
    // ── Construction ─────────────────────────────────────────────────────────

    /// @brief Default constructor. Creates an empty deque with no allocation.
    /// @note `map_` is `nullptr`; the first push triggers allocation.
    /// @complexity O(1).
    Deque() noexcept : map_(nullptr), map_cap_(0),
        map_begin_(0), map_end_(0),
        front_offset_(0), back_offset_(0), size_(0) {}

    /// @brief Copy constructor. Deep-copies all elements from @p o.
    ///
    /// Pre-reserves enough map slots to hold the same number of blocks as @p o
    /// before iterating and pushing each element.
    /// @param o Source deque to copy from.
    /// @complexity O(n).
    Deque(const Deque& o) : Deque() {
        reserve_map(o.map_end_ - o.map_begin_ + 2);
        for (const auto& v : o) push_back(v);
    }

    /// @brief Move constructor. Transfers all internal state from @p o.
    /// @param o Source deque to move from. Left with a null map and zero size.
    /// @complexity O(1).
    Deque(Deque&& o) noexcept
        : map_(o.map_), map_cap_(o.map_cap_),
          map_begin_(o.map_begin_), map_end_(o.map_end_),
          front_offset_(o.front_offset_), back_offset_(o.back_offset_),
          size_(o.size_)
    {
        o.map_ = nullptr; o.map_cap_ = o.map_begin_ = o.map_end_ = o.size_ = 0;
    }

    /// @brief Destructor. Destroys all elements and frees all blocks and the map.
    /// @complexity O(n).
    ~Deque() { clear(); free_map(); }

    /// @brief Copy-assignment operator. Replaces contents with a copy of @p o.
    ///
    /// Implemented via copy-and-swap for strong exception safety.
    /// @param o Source deque to copy from.
    /// @return Reference to `*this`.
    /// @complexity O(n).
    Deque& operator=(const Deque& o) {
        if (this != &o) { Deque tmp(o); *this = traits::move(tmp); }
        return *this;
    }

    /// @brief Move-assignment operator. Transfers all state from @p o.
    /// @param o Source deque to move from. Left with a null map and zero size.
    /// @return Reference to `*this`.
    /// @complexity O(n) to destroy current elements; O(1) for the transfer.
    Deque& operator=(Deque&& o) noexcept {
        if (this != &o) {
            clear(); free_map();
            map_ = o.map_; map_cap_ = o.map_cap_;
            map_begin_ = o.map_begin_; map_end_ = o.map_end_;
            front_offset_ = o.front_offset_; back_offset_ = o.back_offset_;
            size_ = o.size_;
            o.map_ = nullptr; o.map_cap_ = o.map_begin_ = o.map_end_ = o.size_ = 0;
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────

    /// @brief Returns the number of elements currently in the deque.
    /// @return Current element count.
    /// @complexity O(1).
    [[nodiscard]] usize size()  const noexcept { return size_; }

    /// @brief Returns `true` if the deque contains no elements.
    /// @return `true` when `size() == 0`.
    /// @complexity O(1).
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Element access ────────────────────────────────────────────────────────

    /// @brief Unchecked random access by logical index (mutable).
    ///
    /// Translates the logical index @p i into a block index and intra-block
    /// offset using the formula:
    ///   `abs = front_offset_ + i`,  `block = map_begin_ + abs / BLOCK`,
    ///   `slot = abs % BLOCK`.
    /// @param i Zero-based logical index.
    /// @return Mutable reference to the element at position @p i.
    /// @pre `i < size()`. No bounds checking is performed.
    /// @complexity O(1).
    T& operator[](usize i) noexcept {
        usize abs = front_offset_ + i;
        usize bi  = map_begin_ + abs / BLOCK;
        usize ei  = abs % BLOCK;
        return map_[bi][ei];
    }

    /// @brief Unchecked random access by logical index (const).
    ///
    /// Same two-level index arithmetic as the mutable overload.
    /// @param i Zero-based logical index.
    /// @return Const reference to the element at position @p i.
    /// @pre `i < size()`. No bounds checking is performed.
    /// @complexity O(1).
    const T& operator[](usize i) const noexcept {
        usize abs = front_offset_ + i;
        usize bi  = map_begin_ + abs / BLOCK;
        usize ei  = abs % BLOCK;
        return map_[bi][ei];
    }

    /// @brief Returns a mutable reference to the first element.
    /// @return Reference to `map_[map_begin_][front_offset_]`.
    /// @pre `!empty()`.
    /// @complexity O(1).
    T&       front()       noexcept { return map_[map_begin_][front_offset_]; }

    /// @brief Returns a const reference to the first element.
    /// @return Const reference to `map_[map_begin_][front_offset_]`.
    /// @pre `!empty()`.
    /// @complexity O(1).
    const T& front() const noexcept { return map_[map_begin_][front_offset_]; }

    /// @brief Returns a mutable reference to the last element.
    ///
    /// Accounts for the edge case where `back_offset_` is 0 (the last element
    /// sits at the end of the previous block).
    /// @return Mutable reference to the last element.
    /// @pre `!empty()`.
    /// @complexity O(1).
    T&       back()        noexcept {
        usize bo = back_offset_ == 0 ? BLOCK - 1 : back_offset_ - 1;
        usize bi = back_offset_ == 0 ? map_end_ - 2 : map_end_ - 1;
        return map_[bi][bo];
    }

    /// @brief Returns a const reference to the last element.
    ///
    /// Accounts for the edge case where `back_offset_` is 0.
    /// @return Const reference to the last element.
    /// @pre `!empty()`.
    /// @complexity O(1).
    const T& back() const noexcept {
        usize bo = back_offset_ == 0 ? BLOCK - 1 : back_offset_ - 1;
        usize bi = back_offset_ == 0 ? map_end_ - 2 : map_end_ - 1;
        return map_[bi][bo];
    }

    // ── Modifiers ─────────────────────────────────────────────────────────────

    /// @brief Appends a copy of @p v to the back of the deque.
    /// @param v Value to copy-append.
    /// @note Allocates a new back block when the current one is full.
    /// @complexity O(1) amortised.
    void push_back(const T& v) { push_back_impl(v); }

    /// @brief Appends @p v to the back of the deque by move.
    /// @param v Value to move-append.
    /// @note Allocates a new back block when the current one is full.
    /// @complexity O(1) amortised.
    void push_back(T&& v)      { push_back_impl(traits::move(v)); }

    /// @brief Prepends a copy of @p v to the front of the deque.
    /// @param v Value to copy-prepend.
    /// @note Allocates a new front block when the current one is exhausted.
    /// @complexity O(1) amortised.
    void push_front(const T& v) { push_front_impl(v); }

    /// @brief Prepends @p v to the front of the deque by move.
    /// @param v Value to move-prepend.
    /// @note Allocates a new front block when the current one is exhausted.
    /// @complexity O(1) amortised.
    void push_front(T&& v)      { push_front_impl(traits::move(v)); }

    /// @brief Constructs a new element in-place at the back of the deque.
    /// @tparam Args Constructor argument types forwarded to `T`'s constructor.
    /// @param  args Arguments forwarded to the constructor of `T`.
    /// @return Mutable reference to the newly constructed element.
    /// @note Allocates a new back block when the current one is full.
    /// @complexity O(1) amortised.
    template<typename... Args>
    T& emplace_back(Args&&... args) {
        ensure_back_space();
        T& ref = map_[map_end_ - 1][back_offset_];
        Allocator<T>::construct(&ref, traits::forward<Args>(args)...);
        ++back_offset_;
        if (back_offset_ == BLOCK) { back_offset_ = 0; }
        ++size_;
        return ref;
    }

    /// @brief Removes the last element.
    ///
    /// If the last block becomes empty after the removal it is deallocated.
    /// @note Has no effect if the deque is empty.
    /// @complexity O(1).
    void pop_back() noexcept {
        if (empty()) return;
        if (back_offset_ == 0) { back_offset_ = BLOCK; --map_end_; }
        --back_offset_;
        Allocator<T>::destroy(&map_[map_end_ - 1][back_offset_]);
        --size_;
        if (map_end_ > map_begin_ + 1 && back_offset_ == 0) {
            Allocator<T>::deallocate(map_[map_end_ - 1]);
            --map_end_;
            back_offset_ = BLOCK;
        }
    }

    /// @brief Removes the first element.
    ///
    /// If the first block becomes empty after the removal it is deallocated.
    /// @note Has no effect if the deque is empty.
    /// @complexity O(1).
    void pop_front() noexcept {
        if (empty()) return;
        Allocator<T>::destroy(&map_[map_begin_][front_offset_]);
        ++front_offset_;
        --size_;
        if (front_offset_ == BLOCK) {
            Allocator<T>::deallocate(map_[map_begin_]);
            ++map_begin_;
            front_offset_ = 0;
        }
    }

    /// @brief Destroys all elements and deallocates all data blocks.
    ///
    /// The map array itself is retained. Call the destructor (or assign a new
    /// deque) to release the map.
    /// @complexity O(n) for non-trivially destructible `T`;
    ///             O(number of blocks) for trivially destructible `T`.
    void clear() noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) {
            for (usize i = 0; i < size_; ++i) Allocator<T>::destroy(&(*this)[i]);
        }
        for (usize b = map_begin_; b < map_end_; ++b) {
            if (map_ && map_[b]) { Allocator<T>::deallocate(map_[b]); map_[b] = nullptr; }
        }
        map_begin_ = map_end_ = 0;
        front_offset_ = back_offset_ = 0;
        size_ = 0;
    }

    // ── Simple forward iterator ───────────────────────────────────────────────

    /// @brief Mutable forward iterator over `Deque<T, BLOCK>` elements.
    ///
    /// Indexes into the deque via its `operator[]`, so each dereference
    /// performs the two-level map lookup. Incrementing advances the logical
    /// index by one.
    struct Iterator {
        /// @brief Pointer to the owning deque.
        Deque* dq;
        /// @brief Current logical index into the deque.
        usize  i;

        /// @brief Dereferences the iterator to obtain the current element (mutable).
        /// @return Mutable reference to `(*dq)[i]`.
        T&   operator*()  noexcept { return (*dq)[i]; }

        /// @brief Returns a pointer to the current element.
        /// @return Pointer to `(*dq)[i]`.
        T*   operator->() noexcept { return &(*dq)[i]; }

        /// @brief Pre-increment: advances to the next element.
        /// @return Reference to `*this` after incrementing.
        Iterator& operator++() noexcept { ++i; return *this; }

        /// @brief Post-increment: advances to the next element, returns old position.
        /// @return Iterator copy pointing to the element before the advance.
        Iterator  operator++(int) noexcept { auto t = *this; ++i; return t; }

        /// @brief Tests iterator equality.
        /// @param o Iterator to compare with.
        /// @return `true` if both iterators have the same logical index.
        bool operator==(Iterator o) const noexcept { return i == o.i; }

        /// @brief Tests iterator inequality.
        /// @param o Iterator to compare with.
        /// @return `true` if the iterators have different logical indices.
        bool operator!=(Iterator o) const noexcept { return i != o.i; }
    };

    /// @brief Immutable forward iterator over `Deque<T, BLOCK>` elements.
    ///
    /// Identical to `Iterator` but yields `const T&` / `const T*`, allowing
    /// read-only traversal of `const Deque` instances.
    struct ConstIterator {
        /// @brief Pointer to the (const) owning deque.
        const Deque* dq;
        /// @brief Current logical index into the deque.
        usize i;

        /// @brief Dereferences the iterator to obtain the current element (const).
        /// @return Const reference to `(*dq)[i]`.
        const T& operator*()  const noexcept { return (*dq)[i]; }

        /// @brief Returns a const pointer to the current element.
        /// @return Const pointer to `(*dq)[i]`.
        const T* operator->() const noexcept { return &(*dq)[i]; }

        /// @brief Pre-increment: advances to the next element.
        /// @return Reference to `*this` after incrementing.
        ConstIterator& operator++() noexcept { ++i; return *this; }

        /// @brief Post-increment: advances to the next element, returns old position.
        /// @return ConstIterator copy pointing to the element before the advance.
        ConstIterator  operator++(int) noexcept { auto t = *this; ++i; return t; }

        /// @brief Tests const iterator equality.
        /// @param o Const iterator to compare with.
        /// @return `true` if both iterators have the same logical index.
        bool operator==(ConstIterator o) const noexcept { return i == o.i; }

        /// @brief Tests const iterator inequality.
        /// @param o Const iterator to compare with.
        /// @return `true` if the iterators have different logical indices.
        bool operator!=(ConstIterator o) const noexcept { return i != o.i; }
    };

    /// @brief Returns a mutable iterator to the first element.
    /// @return `Iterator` with logical index 0.
    /// @complexity O(1).
    Iterator      begin()        noexcept { return {this, 0}; }

    /// @brief Returns a mutable past-the-end iterator.
    /// @return `Iterator` with logical index equal to `size()`.
    /// @complexity O(1).
    Iterator      end()          noexcept { return {this, size_}; }

    /// @brief Returns a const iterator to the first element.
    /// @return `ConstIterator` with logical index 0.
    /// @complexity O(1).
    ConstIterator begin()  const noexcept { return {this, 0}; }

    /// @brief Returns a const past-the-end iterator.
    /// @return `ConstIterator` with logical index equal to `size()`.
    /// @complexity O(1).
    ConstIterator end()    const noexcept { return {this, size_}; }

    /// @brief Returns a const iterator to the first element (explicit const).
    /// @return Same as `begin() const`.
    /// @complexity O(1).
    ConstIterator cbegin() const noexcept { return begin(); }

    /// @brief Returns a const past-the-end iterator (explicit const).
    /// @return Same as `end() const`.
    /// @complexity O(1).
    ConstIterator cend()   const noexcept { return end(); }

private:
    /// @brief Releases the map array and resets map metadata to zero.
    /// @note Does NOT destroy or free the data blocks — call `clear()` first.
    /// @complexity O(1).
    void free_map() noexcept {
        if (map_) {
            Allocator<T*>::deallocate(reinterpret_cast<T**>(map_), map_cap_);
            map_ = nullptr; map_cap_ = 0;
        }
    }

    /// @brief Ensures the map has at least @p needed slots, reallocating if necessary.
    ///
    /// The new map is allocated with `max(MIN_MAP, needed * 2)` slots to
    /// amortise future growth. Existing block pointers are copied into the
    /// centre of the new map so that equal headroom is available at both ends.
    ///
    /// @param needed Minimum number of map slots required.
    /// @complexity O(map_cap_) — copies all existing block pointers.
    void reserve_map(usize needed) {
        if (needed <= map_cap_) return;
        usize nc = needed < MIN_MAP ? MIN_MAP : needed * 2;
        T** nm = mem::alloc_n<T*>(nc);
        mem::zero(nm, nc * sizeof(T*));
        if (map_) {
            mem::copy(nm + (nc - map_cap_) / 2, map_, map_cap_ * sizeof(T*));
            usize offset = (nc - map_cap_) / 2;
            mem::dealloc_n(map_);
            map_begin_ += offset;
            map_end_   += offset;
        }
        map_ = nm; map_cap_ = nc;
    }

    /// @brief Ensures there is an available slot at the back for the next element.
    ///
    /// Allocates a new block at `map_[map_end_]` when `back_offset_` wraps to
    /// 0 (meaning the current last block is full). Grows the map first if
    /// `map_end_ == map_cap_`. On the very first push into an empty deque the
    /// map and first block are both initialised with the logical cursor placed
    /// in the middle of that block.
    ///
    /// @complexity O(1) amortised (map growth is O(map_cap_) but rare).
    void ensure_back_space() {
        if (map_end_ == 0 || back_offset_ == 0) {
            // Need a new block at the back
            if (map_end_ == map_cap_) reserve_map(map_cap_ + 1);
            if (map_end_ == map_begin_ && size_ == 0) {
                map_begin_ = map_cap_ / 2;
                map_end_   = map_begin_;
                front_offset_ = BLOCK / 2;
                back_offset_  = BLOCK / 2;
            }
            map_[map_end_] = Allocator<T>::allocate(BLOCK);
            ++map_end_;
            back_offset_ = 0;
        }
    }

    /// @brief Ensures there is an available slot at the front for the next element.
    ///
    /// Allocates a new block at `map_[map_begin_ - 1]` when `front_offset_`
    /// is 0 (meaning the current first block has no room ahead of its cursor).
    /// Grows the map first if `map_begin_ == 0`.
    ///
    /// @complexity O(1) amortised (map growth is O(map_cap_) but rare).
    void ensure_front_space() {
        if (front_offset_ == 0) {
            if (map_begin_ == 0) reserve_map(map_cap_ + 1);
            --map_begin_;
            map_[map_begin_] = Allocator<T>::allocate(BLOCK);
            front_offset_ = BLOCK;
        }
    }

    /// @brief Shared implementation for `push_back(const T&)` and `push_back(T&&)`.
    /// @tparam U   Deduced type of the value (lvalue or rvalue reference).
    /// @param v    Forwarding reference to the value to append.
    /// @complexity O(1) amortised.
    template<typename U>
    void push_back_impl(U&& v) {
        ensure_back_space();
        Allocator<T>::construct(&map_[map_end_ - 1][back_offset_], traits::forward<U>(v));
        ++back_offset_;
        if (back_offset_ == BLOCK) { back_offset_ = 0; }
        ++size_;
    }

    /// @brief Shared implementation for `push_front(const T&)` and `push_front(T&&)`.
    /// @tparam U   Deduced type of the value (lvalue or rvalue reference).
    /// @param v    Forwarding reference to the value to prepend.
    /// @complexity O(1) amortised.
    template<typename U>
    void push_front_impl(U&& v) {
        ensure_front_space();
        --front_offset_;
        Allocator<T>::construct(&map_[map_begin_][front_offset_], traits::forward<U>(v));
        ++size_;
    }
};

} // namespace dsc
