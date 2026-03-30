#pragma once

#include "primitives.hpp"

// Raw memory operations and allocation — no libc, no STL.
// Uses GCC/Clang __builtin_* intrinsics (accepted by MSVC via __assume hints
// as a fallback byte-loop).

/// @brief Sub-namespace containing raw memory primitives for the dsc library.
/// @note  All functions avoid the C and C++ standard libraries entirely.
///        On GCC/Clang, compiler built-ins (`__builtin_memcpy`, etc.) are used
///        so the optimizer can generate optimal machine code.  On other compilers
///        a portable byte-loop fallback is compiled instead.
namespace dsc::mem {

// ── Raw byte ops ──────────────────────────────────────────────────────────────

/// @brief Copy `n` bytes from `src` to `dst` (non-overlapping regions only).
/// @param dst  Destination buffer; must not overlap with `src`.
/// @param src  Source buffer; must not overlap with `dst`.
/// @param n    Number of bytes to copy.
/// @return     `dst`.
/// @pre    `dst` and `src` must each point to at least `n` valid bytes.
/// @pre    The memory regions `[dst, dst+n)` and `[src, src+n)` must not overlap.
/// @warning Overlapping regions produce undefined behaviour; use `move_bytes` instead.
/// @complexity O(n)
[[nodiscard]] inline void* copy(void* __restrict dst,
                                 const void* __restrict src,
                                 usize n) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_memcpy(dst, src, n);
#else
    auto* d = reinterpret_cast<u8*>(dst);
    const auto* s = reinterpret_cast<const u8*>(src);
    for (usize i = 0; i < n; ++i) d[i] = s[i];
    return dst;
#endif
}

/// @brief Copy `n` bytes from `src` to `dst`, handling overlapping regions safely.
/// @param dst  Destination buffer.
/// @param src  Source buffer.
/// @param n    Number of bytes to move.
/// @return     `dst`.
/// @pre    Both `dst` and `src` must point to at least `n` valid bytes.
/// @note   Safe to call when `[dst, dst+n)` and `[src, src+n)` overlap.
/// @complexity O(n)
inline void* move_bytes(void* dst, const void* src, usize n) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_memmove(dst, src, n);
#else
    auto* d = reinterpret_cast<u8*>(dst);
    const auto* s = reinterpret_cast<const u8*>(src);
    if (d < s)       for (usize i = 0;     i < n; ++i) d[i] = s[i];
    else if (d > s)  for (usize i = n; i-- > 0;)       d[i] = s[i];
    return dst;
#endif
}

/// @brief Fill `n` bytes starting at `dst` with the value zero.
/// @param dst  Destination buffer to zero-fill.
/// @param n    Number of bytes to set to zero.
/// @return     `dst`.
/// @pre    `dst` must point to at least `n` valid, writable bytes.
/// @complexity O(n)
inline void* zero(void* dst, usize n) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_memset(dst, 0, n);
#else
    auto* d = reinterpret_cast<u8*>(dst);
    for (usize i = 0; i < n; ++i) d[i] = 0;
    return dst;
#endif
}

/// @brief Lexicographically compare `n` bytes from two memory regions.
/// @param a  Pointer to the first memory region.
/// @param b  Pointer to the second memory region.
/// @param n  Number of bytes to compare.
/// @return   Negative if `a < b`, zero if equal, positive if `a > b`
///           (treating each byte as `unsigned char`).
/// @pre    Both `a` and `b` must point to at least `n` valid bytes.
/// @complexity O(n)
inline int compare(const void* a, const void* b, usize n) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_memcmp(a, b, n);
#else
    const auto* p = reinterpret_cast<const u8*>(a);
    const auto* q = reinterpret_cast<const u8*>(b);
    for (usize i = 0; i < n; ++i) {
        if (p[i] < q[i]) return -1;
        if (p[i] > q[i]) return  1;
    }
    return 0;
#endif
}

// ── Heap allocation ───────────────────────────────────────────────────────────
// operator new / operator delete are provided by the C++ runtime (not STL).

/// @brief Allocate `bytes` bytes of raw (untyped) heap memory.
/// @param bytes  Number of bytes to allocate.
/// @return       Pointer to the newly allocated memory; never `nullptr`.
/// @note  Delegates to global `::operator new`, which is provided by the C++
///        runtime — no STL involvement.
/// @warning Throws `std::bad_alloc` (or terminates, depending on the runtime)
///          if the allocation fails.
/// @complexity O(1) amortised (implementation-defined)
[[nodiscard]] inline void* alloc(usize bytes) {
    return ::operator new(bytes);
}

/// @brief Deallocate a block previously obtained from `alloc`.
/// @param ptr  Pointer returned by a prior call to `alloc`; may be `nullptr`.
/// @note  Delegates to global `::operator delete`.
/// @complexity O(1) amortised (implementation-defined)
inline void dealloc(void* ptr) noexcept {
    ::operator delete(ptr);
}

// Typed allocation helpers

/// @brief Allocate storage for `n` objects of type `T` (no construction).
/// @tparam T  Element type.
/// @param  n  Number of elements to allocate space for.
/// @return    Typed pointer to raw storage for `n` objects of type `T`.
/// @warning   Objects are not constructed; call placement `new` before use.
/// @complexity O(1) amortised (implementation-defined)
template<typename T>
[[nodiscard]] T* alloc_n(usize n) {
    return static_cast<T*>(::operator new(n * sizeof(T)));
}

/// @brief Deallocate a typed block previously obtained from `alloc_n<T>`.
/// @tparam T   Element type.
/// @param  ptr Pointer returned by a prior call to `alloc_n<T>`; may be `nullptr`.
/// @warning    Objects must already have been destroyed before calling this.
/// @complexity O(1) amortised (implementation-defined)
template<typename T>
void dealloc_n(T* ptr) noexcept {
    ::operator delete(static_cast<void*>(ptr));
}

} // namespace dsc::mem
