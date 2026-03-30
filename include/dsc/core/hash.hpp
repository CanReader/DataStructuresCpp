#pragma once

#include "primitives.hpp"
#include "traits.hpp"

namespace dsc {

// ── FNV-1a hash ───────────────────────────────────────────────────────────────
// Fast, high-quality non-cryptographic hash — no STL required.
// Used as the default hash for byte-level types.

/// @brief Implementation details for the hashing subsystem; not part of the public API.
namespace detail {

/// @brief FNV-1a 64-bit prime multiplier.
inline constexpr u64 FNV_PRIME  = 0x00000100000001B3ULL;
/// @brief FNV-1a 64-bit offset basis (initial hash value).
inline constexpr u64 FNV_OFFSET = 0xcbf29ce484222325ULL;

/// @brief Compute a 64-bit FNV-1a hash over `len` raw bytes starting at `data`.
/// @param data  Pointer to the first byte of the input.
/// @param len   Number of bytes to hash.
/// @return      64-bit FNV-1a hash of the input bytes.
/// @note   FNV-1a is a fast, non-cryptographic hash with good avalanche properties
///         for short keys.  It is NOT suitable for cryptographic use.
/// @complexity O(len)
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

/// @brief Apply a finalisation mix (based on Fibonacci/golden-ratio constants) to a hash value.
///
/// Mixes and avalanches all bits of `h` to prevent degenerate clustering in
/// power-of-two hash tables when keys are simple integers.
///
/// @param h  Raw hash value to mix.
/// @return   Well-avalanched 64-bit hash value.
/// @complexity O(1)
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

/// @brief Default hash functor for arbitrary type `T`.
///
/// The primary template hashes `T` byte-by-byte using FNV-1a followed by the
/// bit-mixing finaliser.  Specialisations are provided for all built-in integer
/// types, `float`, `double`, and pointer types to avoid memory indirection.
///
/// @tparam T  The type to hash.
/// @note  `T` must be a trivially copyable type for the byte-level hash to be
///        well-defined (padding bytes may affect the result).
template<typename T>
struct Hash {
    /// @brief Compute a 64-bit hash of `v`.
    /// @param v  Value to hash; hashed byte-by-byte via FNV-1a.
    /// @return   64-bit hash value.
    /// @complexity O(sizeof(T))
    [[nodiscard]] u64 operator()(const T& v) const noexcept {
        return detail::mix(detail::fnv1a(&v, sizeof(T)));
    }
};

// Specialisations for common integer types — avoid memory indirection.

/// @brief Helper macro that generates a `Hash` specialisation for a built-in integer type.
///
/// The specialisation casts the value to `u64` and applies the bit-mixing finaliser
/// directly, avoiding any memory indirection.
///
/// @param type  A built-in integer type (e.g. `int`, `unsigned long`).
#define DSC_HASH_INT(type) \
    template<> struct Hash<type> { \
        [[nodiscard]] u64 operator()(type v) const noexcept { \
            return detail::mix(static_cast<u64>(v)); \
        } \
    };

/// @cond SPECIALISATIONS
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
/// @endcond
#undef DSC_HASH_INT

// Float/double — hash the raw bits

/// @brief `Hash` specialisation for `float` — hashes the raw IEEE 754 bit pattern.
/// @note  All NaN representations hash to distinct values; `+0.0f` and `-0.0f`
///        are distinct bit patterns and will hash differently.
template<> struct Hash<float> {
    /// @brief Compute a 64-bit hash of the raw bit representation of `v`.
    /// @param v  `float` value to hash.
    /// @return   64-bit hash value.
    /// @complexity O(1)
    [[nodiscard]] u64 operator()(float v) const noexcept {
        u32 bits; __builtin_memcpy(&bits, &v, 4);
        return detail::mix(static_cast<u64>(bits));
    }
};

/// @brief `Hash` specialisation for `double` — hashes the raw IEEE 754 bit pattern.
/// @note  All NaN representations hash to distinct values; `+0.0` and `-0.0`
///        are distinct bit patterns and will hash differently.
template<> struct Hash<double> {
    /// @brief Compute a 64-bit hash of the raw bit representation of `v`.
    /// @param v  `double` value to hash.
    /// @return   64-bit hash value.
    /// @complexity O(1)
    [[nodiscard]] u64 operator()(double v) const noexcept {
        u64 bits; __builtin_memcpy(&bits, &v, 8);
        return detail::mix(bits);
    }
};

// Pointer

/// @brief `Hash` specialisation for pointer types — hashes the raw address.
/// @tparam T  Pointee type.
template<typename T>
struct Hash<T*> {
    /// @brief Compute a 64-bit hash of the pointer address `p`.
    /// @param p  Pointer value to hash.
    /// @return   64-bit hash value derived from the numeric address.
    /// @complexity O(1)
    [[nodiscard]] u64 operator()(T* p) const noexcept {
        return detail::mix(reinterpret_cast<usize>(p));
    }
};

// ── KeyEqual<T> ───────────────────────────────────────────────────────────────

/// @brief Default equality comparator for hash-table keys of type `T`.
///
/// Uses `operator==` for comparison.  Can be replaced with a custom comparator
/// as a template argument to dsc hash containers.
///
/// @tparam T  The key type to compare.
template<typename T>
struct KeyEqual {
    /// @brief Compare two keys for equality using `operator==`.
    /// @param a  Left-hand key.
    /// @param b  Right-hand key.
    /// @return   `true` if `a == b`, `false` otherwise.
    /// @complexity O(1) for scalar types; depends on `T::operator==` otherwise.
    [[nodiscard]] bool operator()(const T& a, const T& b) const noexcept { return a == b; }
};

} // namespace dsc
