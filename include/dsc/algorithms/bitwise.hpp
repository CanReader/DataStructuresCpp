#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../containers/array.hpp"

// ── Bit Manipulation algorithms ───────────────────────────────────────────────
// Essential bit-level operations and algorithms.

namespace dsc {

// ── Bit Counting ──────────────────────────────────────────────────────────────
/// @brief Counts the number of set bits (population count / Hamming weight).
/// @param x Input value.
/// @return Number of 1-bits.
/// @complexity O(log x) or O(1) with builtin.
template<typename T>
[[nodiscard]] inline u32 popcount(T x) noexcept {
    u32 count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

// ── Trailing Zero Count ───────────────────────────────────────────────────────
/// @brief Counts trailing zeros in binary representation.
/// @param x Input value (must be non-zero).
/// @return Position of lowest set bit.
/// @complexity O(log x).
template<typename T>
[[nodiscard]] inline u32 ctz(T x) noexcept {
    if (x == 0) return 0;
    u32 count = 0;
    while ((x & 1) == 0) {
        count++;
        x >>= 1;
    }
    return count;
}

// ── Leading Zero Count ────────────────────────────────────────────────────────
/// @brief Counts leading zeros in binary representation.
/// @param x Input value.
/// @return Number of leading zeros.
/// @complexity O(log x).
template<typename T>
[[nodiscard]] inline u32 clz(T x) noexcept {
    if (x == 0) return sizeof(T) * 8;
    u32 count = 0;
    u32 bits = sizeof(T) * 8;
    T msb = T(1) << (bits - 1);
    while ((x & msb) == 0) {
        count++;
        x <<= 1;
    }
    return count;
}

// ── Check if power of 2 ───────────────────────────────────────────────────────
/// @brief Checks if a number is a power of 2.
/// @param x Input value.
/// @return true if x is a power of 2, false otherwise.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline bool is_power_of_two(T x) noexcept {
    return x > 0 && (x & (x - 1)) == 0;
}

// ── Next Power of 2 ───────────────────────────────────────────────────────────
/// @brief Finds the next power of 2 greater than or equal to x.
/// @param x Input value.
/// @return Smallest power of 2 >= x.
/// @complexity O(log x).
template<typename T>
[[nodiscard]] inline T next_power_of_two(T x) noexcept {
    if (x == 0) return 1;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    if (sizeof(T) > 4) x |= x >> 32;
    return x + 1;
}

// ── Reverse Bits ──────────────────────────────────────────────────────────────
/// @brief Reverses the bits of a number.
/// @param x Input value.
/// @return Bitwise reversed value.
/// @complexity O(log x).
template<typename T>
[[nodiscard]] inline T reverse_bits(T x) noexcept {
    T result = 0;
    u32 bits = sizeof(T) * 8;
    for (u32 i = 0; i < bits; ++i) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

// ── Gray Code ──────────────────────────────────────────────────────────────────
/// @brief Converts a binary number to Gray code.
/// @param x Input value.
/// @return Gray code representation.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline T to_gray(T x) noexcept {
    return x ^ (x >> 1);
}

/// @brief Converts Gray code back to binary.
/// @param x Gray code value.
/// @return Binary representation.
/// @complexity O(log x).
template<typename T>
[[nodiscard]] inline T from_gray(T x) noexcept {
    T result = x;
    x >>= 1;
    while (x) {
        result ^= x;
        x >>= 1;
    }
    return result;
}

// ── Subset Enumeration ────────────────────────────────────────────────────────
/// @brief Gets the next subset in lexicographic order (bitmask iteration).
/// @param mask Current subset mask.
/// @param full Superset mask.
/// @return Next subset; returns 0 when wrapping around.
/// @complexity O(1) amortized.
template<typename T>
[[nodiscard]] inline T next_subset(T mask, T full) noexcept {
    return (mask - 1) & full;
}

// ── Hamming Distance ──────────────────────────────────────────────────────────
/// @brief Computes Hamming distance between two values (number of differing bits).
/// @param x First value.
/// @param y Second value.
/// @return Number of bit positions where x and y differ.
/// @complexity O(log x) or O(1) with builtin.
template<typename T>
[[nodiscard]] inline u32 hamming_distance(T x, T y) noexcept {
    return popcount(x ^ y);
}

// ── Lowest Set Bit ───────────────────────────────────────────────────────────
/// @brief Isolates the lowest set bit.
/// @param x Input value.
/// @return Value with only the lowest set bit set.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline T lowest_set_bit(T x) noexcept {
    return x & -x;
}

// ── Bit Set Operations ────────────────────────────────────────────────────────
/// @brief Sets a bit at position i.
/// @param x Input value.
/// @param i Bit position.
/// @return Value with bit i set.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline T set_bit(T x, u32 i) noexcept {
    return x | (T(1) << i);
}

/// @brief Clears a bit at position i.
/// @param x Input value.
/// @param i Bit position.
/// @return Value with bit i cleared.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline T clear_bit(T x, u32 i) noexcept {
    return x & ~(T(1) << i);
}

/// @brief Toggles a bit at position i.
/// @param x Input value.
/// @param i Bit position.
/// @return Value with bit i toggled.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline T toggle_bit(T x, u32 i) noexcept {
    return x ^ (T(1) << i);
}

/// @brief Tests if bit i is set.
/// @param x Input value.
/// @param i Bit position.
/// @return true if bit i is set, false otherwise.
/// @complexity O(1).
template<typename T>
[[nodiscard]] inline bool test_bit(T x, u32 i) noexcept {
    return (x & (T(1) << i)) != 0;
}

} // namespace dsc
