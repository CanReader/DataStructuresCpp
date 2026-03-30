#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"

namespace dsc {

// ── Queue<T> ──────────────────────────────────────────────────────────────────
// FIFO container implemented as a circular buffer.
//
// Design:
//   • head_ points to the oldest element; tail_ to the next write slot.
//   • Capacity is always a power of two, enabling fast modulo via bitmask.
//   • On resize (full), capacity doubles — all data is copied/moved
//     linearly to the new buffer (correct wrap-around handled).
//   • O(1) enqueue, O(1) dequeue, O(n) resize (amortized O(1) per op).
template<typename T>
class Queue {
public:
    Queue() noexcept : data_(nullptr), head_(0), tail_(0), size_(0), cap_(0) {}

    Queue(const Queue& o) : head_(0), tail_(o.size_), size_(o.size_), cap_(o.size_) {
        if (size_ == 0) { data_ = nullptr; return; }
        cap_ = next_pow2(size_);
        data_ = Allocator<T>::allocate(cap_);
        for (usize i = 0; i < size_; ++i)
            Allocator<T>::construct(data_ + i, o.data_[(o.head_ + i) & (o.cap_ - 1)]);
        tail_ = size_ & (cap_ - 1);
    }

    Queue(Queue&& o) noexcept
        : data_(o.data_), head_(o.head_), tail_(o.tail_), size_(o.size_), cap_(o.cap_)
    {
        o.data_ = nullptr; o.head_ = o.tail_ = o.size_ = o.cap_ = 0;
    }

    ~Queue() { clear(); Allocator<T>::deallocate(data_, cap_); }

    Queue& operator=(const Queue& o) {
        if (this != &o) { Queue tmp(o); *this = traits::move(tmp); }
        return *this;
    }

    Queue& operator=(Queue&& o) noexcept {
        if (this != &o) {
            clear(); Allocator<T>::deallocate(data_, cap_);
            data_ = o.data_; head_ = o.head_; tail_ = o.tail_;
            size_ = o.size_; cap_ = o.cap_;
            o.data_ = nullptr; o.head_ = o.tail_ = o.size_ = o.cap_ = 0;
        }
        return *this;
    }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void enqueue(const T& v) {
        grow_if_full();
        Allocator<T>::construct(data_ + tail_, v);
        tail_ = (tail_ + 1) & (cap_ - 1);
        ++size_;
    }

    void enqueue(T&& v) {
        grow_if_full();
        Allocator<T>::construct(data_ + tail_, traits::move(v));
        tail_ = (tail_ + 1) & (cap_ - 1);
        ++size_;
    }

    template<typename... Args>
    T& emplace(Args&&... args) {
        grow_if_full();
        Allocator<T>::construct(data_ + tail_, traits::forward<Args>(args)...);
        T& ref = data_[tail_];
        tail_ = (tail_ + 1) & (cap_ - 1);
        ++size_;
        return ref;
    }

    // Remove and return front element
    [[nodiscard]] T dequeue() noexcept {
        T v = traits::move(data_[head_]);
        Allocator<T>::destroy(data_ + head_);
        head_ = (head_ + 1) & (cap_ - 1);
        --size_;
        return v;
    }

    void clear() noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) {
            for (usize i = 0; i < size_; ++i)
                Allocator<T>::destroy(data_ + ((head_ + i) & (cap_ - 1)));
        }
        head_ = tail_ = size_ = 0;
    }

    // ── Access ────────────────────────────────────────────────────────────────
    [[nodiscard]] T&       front()       noexcept { return data_[head_]; }
    [[nodiscard]] const T& front() const noexcept { return data_[head_]; }
    [[nodiscard]] T&       back()        noexcept { return data_[(tail_ + cap_ - 1) & (cap_ - 1)]; }
    [[nodiscard]] const T& back()  const noexcept { return data_[(tail_ + cap_ - 1) & (cap_ - 1)]; }

    // ── State ─────────────────────────────────────────────────────────────────
    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }
    [[nodiscard]] bool  full()  const noexcept { return size_ == cap_; }

private:
    T*    data_;
    usize head_;
    usize tail_;
    usize size_;
    usize cap_;

    static usize next_pow2(usize n) noexcept {
        if (n == 0) return 4;
        usize p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    void grow_if_full() {
        if (size_ < cap_) return;
        usize nc = cap_ == 0 ? 4 : cap_ * 2;
        T* nd = Allocator<T>::allocate(nc);
        for (usize i = 0; i < size_; ++i) {
            Allocator<T>::construct(nd + i, traits::move(data_[(head_ + i) & (cap_ - 1)]));
            if constexpr (!traits::is_trivially_destructible_v<T>)
                Allocator<T>::destroy(data_ + ((head_ + i) & (cap_ - 1)));
        }
        Allocator<T>::deallocate(data_, cap_);
        data_ = nd;
        head_ = 0;
        tail_ = size_ & (nc - 1);
        cap_  = nc;
    }
};

} // namespace dsc
