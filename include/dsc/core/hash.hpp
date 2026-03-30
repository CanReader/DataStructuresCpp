#pragma once

#include "primitives.hpp"
#include "traits.hpp"

namespace dsc {

// ── FNV-1a hash ───────────────────────────────────────────────────────────────
// Fast, high-quality non-cryptographic hash — no STL required.
// Used as the default hash for byte-level types.

namespace detail {

inline constexpr u64 FNV_PRIME  = 0x00000100000001B3ULL;
inline constexpr u64 FNV_OFFSET = 0xcbf29ce484222325ULL;

inline u64 fnv1a(const void* data, usize len) noexcept {
    u64 h = FNV_OFFSET;
    const auto* p = reinterpret_cast<const u8*>(data);
    for (usize i = 0; i < len; ++i) {
        h ^= static_cast<u64>(p[i]);
        h *= FNV_PRIME;
    }
    return h;
}

// Fibonacci hashing — mixes/avalanches hash bits well, keeps power-of-2 tables
// from degenerating on simple integer keys.
inline u64 mix(u64 h) noexcept {
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return h;
}

} // namespace detail

// ── Hash<T> ───────────────────────────────────────────────────────────────────
// Primary template: hash byte-by-byte via FNV-1a.
template<typename T>
struct Hash {
    [[nodiscard]] u64 operator()(const T& v) const noexcept {
        return detail::mix(detail::fnv1a(&v, sizeof(T)));
    }
};

// Specialisations for common integer types — avoid memory indirection.
#define DSC_HASH_INT(type) \
    template<> struct Hash<type> { \
        [[nodiscard]] u64 operator()(type v) const noexcept { \
            return detail::mix(static_cast<u64>(v)); \
        } \
    };

DSC_HASH_INT(bool)
DSC_HASH_INT(char)
DSC_HASH_INT(signed char)
DSC_HASH_INT(unsigned char)
DSC_HASH_INT(short)
DSC_HASH_INT(unsigned short)
DSC_HASH_INT(int)
DSC_HASH_INT(unsigned int)
DSC_HASH_INT(long)
DSC_HASH_INT(unsigned long)
DSC_HASH_INT(long long)
DSC_HASH_INT(unsigned long long)
#undef DSC_HASH_INT

// Float/double — hash the raw bits
template<> struct Hash<float> {
    [[nodiscard]] u64 operator()(float v) const noexcept {
        u32 bits; __builtin_memcpy(&bits, &v, 4);
        return detail::mix(static_cast<u64>(bits));
    }
};
template<> struct Hash<double> {
    [[nodiscard]] u64 operator()(double v) const noexcept {
        u64 bits; __builtin_memcpy(&bits, &v, 8);
        return detail::mix(bits);
    }
};

// Pointer
template<typename T>
struct Hash<T*> {
    [[nodiscard]] u64 operator()(T* p) const noexcept {
        return detail::mix(reinterpret_cast<usize>(p));
    }
};

// ── KeyEqual<T> ───────────────────────────────────────────────────────────────
template<typename T>
struct KeyEqual {
    [[nodiscard]] bool operator()(const T& a, const T& b) const noexcept { return a == b; }
};

} // namespace dsc
