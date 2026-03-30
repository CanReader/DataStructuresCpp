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

/// @brief First-in, first-out (FIFO) container implemented as a circular buffer.
///
/// Elements are stored in a single heap allocation. `head_` points to the
/// oldest (next-to-dequeue) element and `tail_` points to the next available
/// write slot. The capacity is always kept at a power of two so that the
/// modulo wrapping (`& (cap_ - 1)`) compiles to a single bitmask instruction.
///
/// When the buffer is full, capacity doubles and all existing elements are
/// moved linearly into the new buffer (wrap-around is handled correctly).
///
/// @tparam T Element type. Must be move-constructible for buffer growth.
template<typename T>
class Queue {
public:
    /// @brief Default constructor. Creates an empty queue with no allocation.
    /// @note `data_` is `nullptr`; the first enqueue triggers the initial
    ///       allocation (capacity 4).
    /// @complexity O(1).
    Queue() noexcept : data_(nullptr), head_(0), tail_(0), size_(0), cap_(0) {}

    /// @brief Copy constructor. Deep-copies all elements from @p o.
    ///
    /// The new queue is laid out linearly (head at index 0) in a fresh
    /// power-of-two–sized buffer.
    /// @param o Source queue to copy from.
    /// @complexity O(n).
    Queue(const Queue& o) : head_(0), tail_(o.size_), size_(o.size_), cap_(o.size_) {
        if (size_ == 0) { data_ = nullptr; return; }
        cap_ = next_pow2(size_);
        data_ = Allocator<T>::allocate(cap_);
        for (usize i = 0; i < size_; ++i)
            Allocator<T>::construct(data_ + i, o.data_[(o.head_ + i) & (o.cap_ - 1)]);
        tail_ = size_ & (cap_ - 1);
    }

    /// @brief Move constructor. Transfers ownership of the buffer from @p o.
    /// @param o Source queue to move from. Left empty after the operation.
    /// @complexity O(1).
    Queue(Queue&& o) noexcept
        : data_(o.data_), head_(o.head_), tail_(o.tail_), size_(o.size_), cap_(o.cap_)
    {
        o.data_ = nullptr; o.head_ = o.tail_ = o.size_ = o.cap_ = 0;
    }

    /// @brief Destructor. Destroys all elements and releases the allocation.
    /// @complexity O(n) for non-trivially destructible `T`; O(1) otherwise.
    ~Queue() { clear(); Allocator<T>::deallocate(data_, cap_); }

    /// @brief Copy-assignment operator. Replaces contents with a copy of @p o.
    ///
    /// Implemented via copy-and-swap for strong exception safety.
    /// @param o Source queue to copy from.
    /// @return Reference to `*this`.
    /// @complexity O(n).
    Queue& operator=(const Queue& o) {
        if (this != &o) { Queue tmp(o); *this = traits::move(tmp); }
        return *this;
    }

    /// @brief Move-assignment operator. Transfers ownership from @p o.
    /// @param o Source queue to move from. Left empty after the operation.
    /// @return Reference to `*this`.
    /// @complexity O(n) to destroy current elements; O(1) for the transfer.
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

    /// @brief Enqueues a copy of @p v at the back of the queue.
    /// @param v Value to copy-enqueue.
    /// @note Triggers a capacity-doubling reallocation when `full()`.
    /// @complexity O(1) amortised.
    void enqueue(const T& v) {
        grow_if_full();
        Allocator<T>::construct(data_ + tail_, v);
        tail_ = (tail_ + 1) & (cap_ - 1);
        ++size_;
    }

    /// @brief Enqueues @p v at the back of the queue by move.
    /// @param v Value to move-enqueue.
    /// @note Triggers a capacity-doubling reallocation when `full()`.
    /// @complexity O(1) amortised.
    void enqueue(T&& v) {
        grow_if_full();
        Allocator<T>::construct(data_ + tail_, traits::move(v));
        tail_ = (tail_ + 1) & (cap_ - 1);
        ++size_;
    }

    /// @brief Constructs a new element in-place at the back of the queue.
    /// @tparam Args Constructor argument types forwarded to `T`'s constructor.
    /// @param  args Arguments forwarded to the constructor of `T`.
    /// @return Mutable reference to the newly constructed element.
    /// @note Triggers a capacity-doubling reallocation when `full()`.
    /// @complexity O(1) amortised.
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

    /// @brief Removes and returns the front (oldest) element by value.
    ///
    /// The element is move-constructed out of the circular buffer slot,
    /// which is then destroyed. `head_` advances by one (with wrap-around).
    /// @return The element that was at the front.
    /// @pre `!empty()`. Behaviour is undefined on an empty queue.
    /// @complexity O(1).
    [[nodiscard]] T dequeue() noexcept {
        T v = traits::move(data_[head_]);
        Allocator<T>::destroy(data_ + head_);
        head_ = (head_ + 1) & (cap_ - 1);
        --size_;
        return v;
    }

    /// @brief Destroys all elements, resetting `size()`, `head_`, and `tail_` to 0.
    /// @note The allocated buffer is retained for reuse.
    /// @complexity O(n) for non-trivially destructible `T`; O(1) otherwise.
    void clear() noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) {
            for (usize i = 0; i < size_; ++i)
                Allocator<T>::destroy(data_ + ((head_ + i) & (cap_ - 1)));
        }
        head_ = tail_ = size_ = 0;
    }

    // ── Access ────────────────────────────────────────────────────────────────

    /// @brief Returns a mutable reference to the front (oldest) element.
    /// @return Reference to the element at `head_`.
    /// @pre `!empty()`.
    /// @complexity O(1).
    [[nodiscard]] T&       front()       noexcept { return data_[head_]; }

    /// @brief Returns a const reference to the front (oldest) element.
    /// @return Const reference to the element at `head_`.
    /// @pre `!empty()`.
    /// @complexity O(1).
    [[nodiscard]] const T& front() const noexcept { return data_[head_]; }

    /// @brief Returns a mutable reference to the back (newest) element.
    /// @return Reference to the most recently enqueued element.
    /// @pre `!empty()`.
    /// @complexity O(1).
    [[nodiscard]] T&       back()        noexcept { return data_[(tail_ + cap_ - 1) & (cap_ - 1)]; }

    /// @brief Returns a const reference to the back (newest) element.
    /// @return Const reference to the most recently enqueued element.
    /// @pre `!empty()`.
    /// @complexity O(1).
    [[nodiscard]] const T& back()  const noexcept { return data_[(tail_ + cap_ - 1) & (cap_ - 1)]; }

    // ── State ─────────────────────────────────────────────────────────────────

    /// @brief Returns the number of elements currently in the queue.
    /// @return Current element count.
    /// @complexity O(1).
    [[nodiscard]] usize size()  const noexcept { return size_; }

    /// @brief Returns `true` if the queue contains no elements.
    /// @return `true` when `size() == 0`.
    /// @complexity O(1).
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    /// @brief Returns `true` if the queue is at full capacity.
    ///
    /// The next enqueue / emplace will trigger a capacity-doubling
    /// reallocation when `full()` returns `true`.
    /// @return `true` when `size() == capacity`.
    /// @complexity O(1).
    [[nodiscard]] bool  full()  const noexcept { return size_ == cap_; }

private:
    T*    data_;
    usize head_;
    usize tail_;
    usize size_;
    usize cap_;

    /// @brief Returns the smallest power of two that is >= @p n.
    ///
    /// Returns 4 for `n == 0` to establish the minimum initial capacity.
    /// @param n Desired minimum value.
    /// @return Smallest power of two >= @p n, or 4 if @p n is 0.
    /// @complexity O(log n).
    static usize next_pow2(usize n) noexcept {
        if (n == 0) return 4;
        usize p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    /// @brief Doubles the buffer capacity when the queue is full.
    ///
    /// Allocates a new buffer of twice the current capacity (minimum 4),
    /// move-constructs all existing elements in logical FIFO order (handling
    /// wrap-around correctly), destroys elements in the old buffer, and
    /// releases the old allocation. After growth `head_` is 0 and `tail_`
    /// equals the old `size_`.
    ///
    /// @note Called automatically by `enqueue` and `emplace` when `full()`.
    /// @complexity O(n).
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
