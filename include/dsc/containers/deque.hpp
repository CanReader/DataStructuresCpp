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
    Deque() noexcept : map_(nullptr), map_cap_(0),
        map_begin_(0), map_end_(0),
        front_offset_(0), back_offset_(0), size_(0) {}

    Deque(const Deque& o) : Deque() {
        reserve_map(o.map_end_ - o.map_begin_ + 2);
        for (const auto& v : o) push_back(v);
    }

    Deque(Deque&& o) noexcept
        : map_(o.map_), map_cap_(o.map_cap_),
          map_begin_(o.map_begin_), map_end_(o.map_end_),
          front_offset_(o.front_offset_), back_offset_(o.back_offset_),
          size_(o.size_)
    {
        o.map_ = nullptr; o.map_cap_ = o.map_begin_ = o.map_end_ = o.size_ = 0;
    }

    ~Deque() { clear(); free_map(); }

    Deque& operator=(const Deque& o) {
        if (this != &o) { Deque tmp(o); *this = traits::move(tmp); }
        return *this;
    }
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
    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Element access ────────────────────────────────────────────────────────
    T& operator[](usize i) noexcept {
        usize abs = front_offset_ + i;
        usize bi  = map_begin_ + abs / BLOCK;
        usize ei  = abs % BLOCK;
        return map_[bi][ei];
    }

    const T& operator[](usize i) const noexcept {
        usize abs = front_offset_ + i;
        usize bi  = map_begin_ + abs / BLOCK;
        usize ei  = abs % BLOCK;
        return map_[bi][ei];
    }

    T&       front()       noexcept { return map_[map_begin_][front_offset_]; }
    const T& front() const noexcept { return map_[map_begin_][front_offset_]; }
    T&       back()        noexcept {
        usize bo = back_offset_ == 0 ? BLOCK - 1 : back_offset_ - 1;
        usize bi = back_offset_ == 0 ? map_end_ - 2 : map_end_ - 1;
        return map_[bi][bo];
    }
    const T& back() const noexcept {
        usize bo = back_offset_ == 0 ? BLOCK - 1 : back_offset_ - 1;
        usize bi = back_offset_ == 0 ? map_end_ - 2 : map_end_ - 1;
        return map_[bi][bo];
    }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void push_back(const T& v) { push_back_impl(v); }
    void push_back(T&& v)      { push_back_impl(traits::move(v)); }

    void push_front(const T& v) { push_front_impl(v); }
    void push_front(T&& v)      { push_front_impl(traits::move(v)); }

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
    struct Iterator {
        Deque* dq;
        usize  i;
        T&   operator*()  noexcept { return (*dq)[i]; }
        T*   operator->() noexcept { return &(*dq)[i]; }
        Iterator& operator++() noexcept { ++i; return *this; }
        Iterator  operator++(int) noexcept { auto t = *this; ++i; return t; }
        bool operator==(Iterator o) const noexcept { return i == o.i; }
        bool operator!=(Iterator o) const noexcept { return i != o.i; }
    };

    struct ConstIterator {
        const Deque* dq;
        usize i;
        const T& operator*()  const noexcept { return (*dq)[i]; }
        const T* operator->() const noexcept { return &(*dq)[i]; }
        ConstIterator& operator++() noexcept { ++i; return *this; }
        ConstIterator  operator++(int) noexcept { auto t = *this; ++i; return t; }
        bool operator==(ConstIterator o) const noexcept { return i == o.i; }
        bool operator!=(ConstIterator o) const noexcept { return i != o.i; }
    };

    Iterator      begin()        noexcept { return {this, 0}; }
    Iterator      end()          noexcept { return {this, size_}; }
    ConstIterator begin()  const noexcept { return {this, 0}; }
    ConstIterator end()    const noexcept { return {this, size_}; }
    ConstIterator cbegin() const noexcept { return begin(); }
    ConstIterator cend()   const noexcept { return end(); }

private:
    void free_map() noexcept {
        if (map_) {
            Allocator<T*>::deallocate(reinterpret_cast<T**>(map_), map_cap_);
            map_ = nullptr; map_cap_ = 0;
        }
    }

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

    void ensure_front_space() {
        if (front_offset_ == 0) {
            if (map_begin_ == 0) reserve_map(map_cap_ + 1);
            --map_begin_;
            map_[map_begin_] = Allocator<T>::allocate(BLOCK);
            front_offset_ = BLOCK;
        }
    }

    template<typename U>
    void push_back_impl(U&& v) {
        ensure_back_space();
        Allocator<T>::construct(&map_[map_end_ - 1][back_offset_], traits::forward<U>(v));
        ++back_offset_;
        if (back_offset_ == BLOCK) { back_offset_ = 0; }
        ++size_;
    }

    template<typename U>
    void push_front_impl(U&& v) {
        ensure_front_space();
        --front_offset_;
        Allocator<T>::construct(&map_[map_begin_][front_offset_], traits::forward<U>(v));
        ++size_;
    }
};

} // namespace dsc
