#pragma once

#include "primitives.hpp"
#include "memory.hpp"
#include "traits.hpp"

namespace dsc {

// ── Default heap allocator ────────────────────────────────────────────────────
// Thin wrapper around operator new/delete with typed helpers.
template<typename T>
struct Allocator {
    using value_type = T;

    [[nodiscard]] static T* allocate(usize n) {
        return mem::alloc_n<T>(n);
    }

    static void deallocate(T* ptr, usize /*n*/ = 0) noexcept {
        mem::dealloc_n(ptr);
    }

    template<typename... Args>
    static void construct(T* ptr, Args&&... args)
        noexcept(traits::is_trivially_constructible_v<T, Args...>)
    {
        new (ptr) T(traits::forward<Args>(args)...);
    }

    static void destroy(T* ptr) noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) ptr->~T();
    }
};

// ── Pool allocator ────────────────────────────────────────────────────────────
// Fixed-capacity free-list allocator.  O(1) alloc and dealloc.
// Ideal for linked list and tree nodes where allocation patterns are
// uniform-sized and highly repeated.
template<typename T, usize Capacity = 512>
class PoolAllocator {
    static_assert(Capacity > 0);
    static_assert(sizeof(T) >= sizeof(void*),
                  "T must be pointer-sized for the free-list to work");

    union Slot {
        alignas(T) unsigned char data[sizeof(T)];
        Slot* next;
    };

    Slot  pool_[Capacity];
    Slot* free_  = nullptr;
    usize used_  = 0;

public:
    using value_type = T;

    PoolAllocator() noexcept {
        for (usize i = 0; i + 1 < Capacity; ++i) pool_[i].next = &pool_[i + 1];
        pool_[Capacity - 1].next = nullptr;
        free_ = &pool_[0];
    }

    PoolAllocator(const PoolAllocator&)            = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    [[nodiscard]] T* allocate(usize n = 1) {
        if (n == 1 && free_) {
            Slot* s = free_;
            free_ = s->next;
            ++used_;
            return reinterpret_cast<T*>(s->data);
        }
        // Overflow: fall back to heap
        return mem::alloc_n<T>(n);
    }

    void deallocate(T* ptr, usize n = 1) noexcept {
        auto* slot = reinterpret_cast<Slot*>(ptr);
        if (n == 1 && slot >= pool_ && slot < pool_ + Capacity) {
            slot->next = free_;
            free_ = slot;
            --used_;
        } else {
            mem::dealloc_n(ptr);
        }
    }

    template<typename... Args>
    void construct(T* ptr, Args&&... args)
        noexcept(traits::is_trivially_constructible_v<T, Args...>)
    {
        new (ptr) T(traits::forward<Args>(args)...);
    }

    void destroy(T* ptr) noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) ptr->~T();
    }

    [[nodiscard]] usize used()      const noexcept { return used_; }
    [[nodiscard]] usize available() const noexcept { return Capacity - used_; }
    [[nodiscard]] usize capacity()  const noexcept { return Capacity; }
};

} // namespace dsc
