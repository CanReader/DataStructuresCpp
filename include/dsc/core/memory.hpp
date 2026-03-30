#pragma once

#include "primitives.hpp"

// Raw memory operations and allocation — no libc, no STL.
// Uses GCC/Clang __builtin_* intrinsics (accepted by MSVC via __assume hints
// as a fallback byte-loop).

namespace dsc::mem {

// ── Raw byte ops ──────────────────────────────────────────────────────────────

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

inline void* zero(void* dst, usize n) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_memset(dst, 0, n);
#else
    auto* d = reinterpret_cast<u8*>(dst);
    for (usize i = 0; i < n; ++i) d[i] = 0;
    return dst;
#endif
}

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

[[nodiscard]] inline void* alloc(usize bytes) {
    return ::operator new(bytes);
}

inline void dealloc(void* ptr) noexcept {
    ::operator delete(ptr);
}

// Typed allocation helpers
template<typename T>
[[nodiscard]] T* alloc_n(usize n) {
    return static_cast<T*>(::operator new(n * sizeof(T)));
}

template<typename T>
void dealloc_n(T* ptr) noexcept {
    ::operator delete(static_cast<void*>(ptr));
}

} // namespace dsc::mem
