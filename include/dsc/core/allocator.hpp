#pragma once

#include "primitives.hpp"
#include "memory.hpp"
#include "traits.hpp"

namespace dsc {

// ── Default heap allocator ────────────────────────────────────────────────────
// Thin wrapper around operator new/delete with typed helpers.

/// @brief Default stateless heap allocator for type `T`.
///
/// Thin wrapper around `operator new` / `operator delete` with typed
/// allocation, deallocation, construction, and destruction helpers.
/// All member functions are `static`; the struct carries no state.
///
/// @tparam T  The element type to allocate and construct.
template<typename T>
struct Allocator {
    /// @brief The element type managed by this allocator.
    using value_type = T;

    /// @brief Allocate raw storage for `n` objects of type `T` (no construction).
    /// @param n  Number of elements to allocate space for.
    /// @return   Typed pointer to uninitialized storage for `n` objects.
    /// @warning  Objects are not constructed; call `construct` before use.
    /// @complexity O(1) amortised
    [[nodiscard]] static T* allocate(usize n) {
        return mem::alloc_n<T>(n);
    }

    /// @brief Deallocate storage previously obtained from `allocate`.
    /// @param ptr  Pointer returned by `allocate`; may be `nullptr`.
    /// @param      (unnamed) Element count; accepted for interface compatibility but ignored.
    /// @warning    Objects must already be destroyed before calling this.
    /// @complexity O(1) amortised
    static void deallocate(T* ptr, usize /*n*/ = 0) noexcept {
        mem::dealloc_n(ptr);
    }

    /// @brief Construct a `T` in-place at `ptr` by forwarding `args` to its constructor.
    /// @tparam Args  Constructor argument types.
    /// @param  ptr   Pre-allocated memory location; must be properly aligned for `T`.
    /// @param  args  Arguments forwarded to the constructor of `T`.
    /// @pre    `ptr` must point to at least `sizeof(T)` bytes of writable, aligned storage.
    /// @complexity O(1)
    template<typename... Args>
    static void construct(T* ptr, Args&&... args)
        noexcept(traits::is_trivially_constructible_v<T, Args...>)
    {
        new (ptr) T(traits::forward<Args>(args)...);
    }

    /// @brief Destroy the `T` object at `ptr` by calling its destructor.
    /// @param ptr  Pointer to a live `T` object to destroy.
    /// @note  If `T` is trivially destructible the destructor call is elided entirely.
    /// @complexity O(1)
    static void destroy(T* ptr) noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) ptr->~T();
    }
};

// ── Pool allocator ────────────────────────────────────────────────────────────
// Fixed-capacity free-list allocator.  O(1) alloc and dealloc.
// Ideal for linked list and tree nodes where allocation patterns are
// uniform-sized and highly repeated.

/// @brief Fixed-capacity free-list pool allocator for type `T`.
///
/// Maintains a compile-time-sized array of `Capacity` slots and a singly-linked
/// free list threaded through them.  Both allocation and deallocation are O(1).
///
/// Ideal for linked-list and tree node allocation where all objects are the same
/// size and the total number of live objects is bounded.  When the pool is
/// exhausted, individual allocations fall back transparently to the global heap.
///
/// @tparam T         The element type whose slots are managed.
/// @tparam Capacity  Maximum number of pool slots; defaults to 512.
///
/// @note  The allocator is non-copyable — each instance owns its pool array.
/// @warning `sizeof(T)` must be at least `sizeof(void*)` so that a free-list
///          pointer can be stored inside an unoccupied slot.
template<typename T, usize Capacity = 512>
class PoolAllocator {
    static_assert(Capacity > 0);
    static_assert(sizeof(T) >= sizeof(void*),
                  "T must be pointer-sized for the free-list to work");

    /// @brief Internal union that overlays aligned object storage with a free-list pointer.
    union Slot {
        /// @brief Raw aligned storage for a single `T` object.
        alignas(T) unsigned char data[sizeof(T)];
        /// @brief Free-list linkage used when the slot is unoccupied.
        Slot* next;
    };

    /// @brief The fixed-size backing array of slots.
    Slot  pool_[Capacity];
    /// @brief Head of the intrusive free list; `nullptr` when the pool is full.
    Slot* free_  = nullptr;
    /// @brief Number of slots currently in use (allocated but not yet deallocated).
    usize used_  = 0;

public:
    /// @brief The element type managed by this allocator.
    using value_type = T;

    /// @brief Constructs the pool and initialises the free list over all `Capacity` slots.
    /// @post  `used() == 0` and `available() == Capacity`.
    /// @complexity O(Capacity)
    PoolAllocator() noexcept {
        for (usize i = 0; i + 1 < Capacity; ++i) pool_[i].next = &pool_[i + 1];
        pool_[Capacity - 1].next = nullptr;
        free_ = &pool_[0];
    }

    /// @brief Copy construction is disabled; the pool array is non-transferable.
    PoolAllocator(const PoolAllocator&)            = delete;
    /// @brief Copy assignment is disabled; the pool array is non-transferable.
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    /// @brief Allocate storage for `n` objects of type `T`.
    ///
    /// When `n == 1` and the free list is non-empty, a pool slot is returned in
    /// O(1).  Otherwise (n > 1 or pool exhausted) the request falls back to the
    /// global heap via `mem::alloc_n`.
    ///
    /// @param n  Number of elements to allocate space for; defaults to 1.
    /// @return   Typed pointer to uninitialized storage.
    /// @warning  Objects are not constructed; call `construct` before use.
    /// @complexity O(1) for single-element pool hits; O(1) amortised for heap fallback.
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

    /// @brief Deallocate storage previously obtained from `allocate`.
    ///
    /// If `ptr` points into the pool array and `n == 1`, the slot is returned to
    /// the free list in O(1).  Otherwise the pointer is forwarded to `mem::dealloc_n`.
    ///
    /// @param ptr  Pointer returned by a prior call to `allocate`.
    /// @param n    Number of elements (must match the value passed to `allocate`); defaults to 1.
    /// @warning    Objects must already be destroyed before calling this.
    /// @complexity O(1)
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

    /// @brief Construct a `T` in-place at `ptr` by forwarding `args` to its constructor.
    /// @tparam Args  Constructor argument types.
    /// @param  ptr   Pre-allocated memory location returned by `allocate`.
    /// @param  args  Arguments forwarded to the constructor of `T`.
    /// @complexity O(1)
    template<typename... Args>
    void construct(T* ptr, Args&&... args)
        noexcept(traits::is_trivially_constructible_v<T, Args...>)
    {
        new (ptr) T(traits::forward<Args>(args)...);
    }

    /// @brief Destroy the `T` object at `ptr` by calling its destructor.
    /// @param ptr  Pointer to a live `T` object to destroy.
    /// @note  If `T` is trivially destructible the destructor call is elided.
    /// @complexity O(1)
    void destroy(T* ptr) noexcept {
        if constexpr (!traits::is_trivially_destructible_v<T>) ptr->~T();
    }

    /// @brief Returns the number of slots currently in use.
    /// @return Number of allocated (live) pool slots.
    [[nodiscard]] usize used()      const noexcept { return used_; }

    /// @brief Returns the number of free slots remaining in the pool.
    /// @return `Capacity - used()`.
    [[nodiscard]] usize available() const noexcept { return Capacity - used_; }

    /// @brief Returns the total number of slots in the pool.
    /// @return The template parameter `Capacity`.
    [[nodiscard]] usize capacity()  const noexcept { return Capacity; }
};

} // namespace dsc
