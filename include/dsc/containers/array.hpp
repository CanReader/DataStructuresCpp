#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/memory.hpp"
#include "../core/allocator.hpp"
#include "../core/iterator.hpp"

namespace dsc {

// ── Array<T, Alloc> ───────────────────────────────────────────────────────────
// Cache-friendly dynamic array (resizable contiguous buffer).
//
// Performance highlights vs std::vector:
//   • Growth factor 1.5× — saves ~25 % peak memory vs 2× while keeping
//     O(1) amortized push_back (at most 1.5n total wasted capacity).
//   • memcpy / memmove fast-path for trivially copyable types; the compiler
//     optimises these into SIMD instructions automatically.
//   • emplace_back constructs in-place — no extra copies.
//   • insert() reallocation fuses move + insert in one pass (no double-move).
template<typename T, typename Alloc = Allocator<T>>
class Array {
public:
    // ── Inner iterator ────────────────────────────────────────────────────────
    struct Iterator : RandomAccessIterator<Iterator, T> {
        T* p;
        explicit Iterator(T* p) noexcept : p(p) {}
        Iterator& advance(isize n)             noexcept { p += n; return *this; }
        T&        deref()                const noexcept { return *p; }
        isize     distance_to(Iterator o) const noexcept { return o.p - p; }
        bool      equal(Iterator o)       const noexcept { return p == o.p; }
    };

    struct ConstIterator : RandomAccessIterator<ConstIterator, const T> {
        const T* p;
        explicit ConstIterator(const T* p) noexcept : p(p) {}
        ConstIterator& advance(isize n)                   noexcept { p += n; return *this; }
        const T&       deref()                      const noexcept { return *p; }
        isize          distance_to(ConstIterator o) const noexcept { return o.p - p; }
        bool           equal(ConstIterator o)       const noexcept { return p == o.p; }
    };

    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = Iterator;
    using const_iterator  = ConstIterator;
    using size_type       = usize;

    // ── Construction ─────────────────────────────────────────────────────────
    Array() noexcept : data_(nullptr), size_(0), cap_(0) {}

    explicit Array(usize n) : size_(n), cap_(n) {
        data_ = Alloc::allocate(n);
        for (usize i = 0; i < n; ++i) Alloc::construct(data_ + i);
    }

    Array(usize n, const T& val) : size_(n), cap_(n) {
        data_ = Alloc::allocate(n);
        for (usize i = 0; i < n; ++i) Alloc::construct(data_ + i, val);
    }

    Array(const Array& o) : size_(o.size_), cap_(o.size_) {
        data_ = Alloc::allocate(cap_);
        copy_construct_range(data_, o.data_, size_);
    }

    Array(Array&& o) noexcept : data_(o.data_), size_(o.size_), cap_(o.cap_) {
        o.data_ = nullptr; o.size_ = o.cap_ = 0;
    }

    ~Array() { clear(); Alloc::deallocate(data_, cap_); }

    Array& operator=(const Array& o) {
        if (this != &o) {
            clear();
            if (cap_ < o.size_) { Alloc::deallocate(data_, cap_); cap_ = o.size_; data_ = Alloc::allocate(cap_); }
            size_ = o.size_;
            copy_construct_range(data_, o.data_, size_);
        }
        return *this;
    }

    Array& operator=(Array&& o) noexcept {
        if (this != &o) {
            clear(); Alloc::deallocate(data_, cap_);
            data_ = o.data_; size_ = o.size_; cap_ = o.cap_;
            o.data_ = nullptr; o.size_ = o.cap_ = 0;
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    [[nodiscard]] usize size()     const noexcept { return size_; }
    [[nodiscard]] usize capacity() const noexcept { return cap_; }
    [[nodiscard]] bool  empty()    const noexcept { return size_ == 0; }

    void reserve(usize n) { if (n > cap_) reallocate(n); }

    void shrink_to_fit() { if (size_ < cap_) reallocate(size_); }

    // ── Element access ────────────────────────────────────────────────────────
    reference       operator[](usize i)       noexcept { return data_[i]; }
    const_reference operator[](usize i) const noexcept { return data_[i]; }
    reference       front()                   noexcept { return data_[0]; }
    const_reference front()             const noexcept { return data_[0]; }
    reference       back()                    noexcept { return data_[size_ - 1]; }
    const_reference back()              const noexcept { return data_[size_ - 1]; }
    pointer         data()                    noexcept { return data_; }
    const_pointer   data()              const noexcept { return data_; }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void push_back(const T& v) { if (size_ == cap_) grow(); Alloc::construct(data_ + size_++, v); }
    void push_back(T&& v)      { if (size_ == cap_) grow(); Alloc::construct(data_ + size_++, traits::move(v)); }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == cap_) grow();
        Alloc::construct(data_ + size_, traits::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back() noexcept { if (size_ > 0) Alloc::destroy(data_ + --size_); }

    // Insert before pos
    Iterator insert(Iterator pos, const T& v)  { return insert_at(pos.p - data_, v); }
    Iterator insert(Iterator pos, T&& v)        { return insert_at(pos.p - data_, traits::move(v)); }

    // Erase single element
    Iterator erase(Iterator pos) noexcept {
        usize i = static_cast<usize>(pos.p - data_);
        Alloc::destroy(data_ + i);
        shift_left(data_ + i, size_ - i - 1);
        --size_;
        return Iterator(data_ + i);
    }

    // Erase range [first, last)
    Iterator erase(Iterator first, Iterator last) noexcept {
        usize f = static_cast<usize>(first.p - data_);
        usize l = static_cast<usize>(last.p  - data_);
        for (usize i = f; i < l; ++i) Alloc::destroy(data_ + i);
        usize n = l - f;
        shift_left(data_ + f, size_ - l);
        size_ -= n;
        return Iterator(data_ + f);
    }

    void resize(usize n) {
        if (n > size_) { reserve(n); for (usize i = size_; i < n; ++i) Alloc::construct(data_ + i); }
        else           { for (usize i = n; i < size_; ++i) Alloc::destroy(data_ + i); }
        size_ = n;
    }

    void resize(usize n, const T& val) {
        if (n > size_) { reserve(n); for (usize i = size_; i < n; ++i) Alloc::construct(data_ + i, val); }
        else           { for (usize i = n; i < size_; ++i) Alloc::destroy(data_ + i); }
        size_ = n;
    }

    void clear() noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>)
            for (usize i = 0; i < size_; ++i) Alloc::destroy(data_ + i);
        size_ = 0;
    }

    // ── Search ────────────────────────────────────────────────────────────────
    template<typename U>
    [[nodiscard]] isize index_of(const U& v) const noexcept {
        for (usize i = 0; i < size_; ++i) if (data_[i] == v) return static_cast<isize>(i);
        return -1;
    }

    template<typename U>
    [[nodiscard]] bool contains(const U& v) const noexcept { return index_of(v) >= 0; }

    // ── Iterators ─────────────────────────────────────────────────────────────
    Iterator      begin()        noexcept { return Iterator(data_); }
    Iterator      end()          noexcept { return Iterator(data_ + size_); }
    ConstIterator begin()  const noexcept { return ConstIterator(data_); }
    ConstIterator end()    const noexcept { return ConstIterator(data_ + size_); }
    ConstIterator cbegin() const noexcept { return begin(); }
    ConstIterator cend()   const noexcept { return end(); }

    // ── Comparison ────────────────────────────────────────────────────────────
    bool operator==(const Array& o) const noexcept {
        if (size_ != o.size_) return false;
        for (usize i = 0; i < size_; ++i) if (!(data_[i] == o.data_[i])) return false;
        return true;
    }
    bool operator!=(const Array& o) const noexcept { return !(*this == o); }

private:
    T*    data_;
    usize size_;
    usize cap_;

    // 1.5× growth, minimum capacity 4
    void grow() { reallocate(cap_ == 0 ? 4 : cap_ + (cap_ >> 1)); }

    void reallocate(usize new_cap) {
        T* nd = Alloc::allocate(new_cap);
        if (data_) {
            move_construct_range(nd, data_, size_);
            Alloc::deallocate(data_, cap_);
        }
        data_ = nd; cap_ = new_cap;
    }

    static void copy_construct_range(T* dst, const T* src, usize n) noexcept {
        if constexpr (traits::is_trivially_copyable_v<T>) {
            mem::copy(dst, src, n * sizeof(T));
        } else {
            for (usize i = 0; i < n; ++i) Alloc::construct(dst + i, src[i]);
        }
    }

    static void move_construct_range(T* dst, T* src, usize n) noexcept {
        if constexpr (traits::is_trivially_copyable_v<T>) {
            mem::copy(dst, src, n * sizeof(T));
        } else {
            for (usize i = 0; i < n; ++i) {
                Alloc::construct(dst + i, traits::move(src[i]));
                Alloc::destroy(src + i);
            }
        }
    }

    static void shift_left(T* base, usize n) noexcept {
        if constexpr (traits::is_trivially_copyable_v<T>) {
            mem::move_bytes(base, base + 1, n * sizeof(T));
        } else {
            for (usize i = 0; i < n; ++i) base[i] = traits::move(base[i + 1]);
        }
    }

    static void shift_right(T* base, usize n) noexcept {
        if constexpr (traits::is_trivially_copyable_v<T>) {
            mem::move_bytes(base + 1, base, n * sizeof(T));
        } else {
            for (usize i = n; i-- > 0;) base[i + 1] = traits::move(base[i]);
        }
    }

    template<typename U>
    Iterator insert_at(usize idx, U&& v) {
        if (size_ == cap_) {
            usize nc = cap_ == 0 ? 4 : cap_ + (cap_ >> 1);
            T* nd = Alloc::allocate(nc);
            move_construct_range(nd, data_, idx);
            Alloc::construct(nd + idx, traits::forward<U>(v));
            move_construct_range(nd + idx + 1, data_ + idx, size_ - idx);
            Alloc::deallocate(data_, cap_);
            data_ = nd; cap_ = nc;
        } else {
            shift_right(data_ + idx, size_ - idx);
            Alloc::construct(data_ + idx, traits::forward<U>(v));
        }
        ++size_;
        return Iterator(data_ + idx);
    }
};

} // namespace dsc
