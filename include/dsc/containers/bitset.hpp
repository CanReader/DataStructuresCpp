#pragma once

#include "../core/primitives.hpp"
#include "../core/memory.hpp"

namespace dsc {

// ── BitSet<N> ─────────────────────────────────────────────────────────────────
// Compile-time fixed-size bitset stored in an array of u64 words.
// Every operation except count() is O(1).
// count() uses __builtin_popcountll — compiles to POPCNT on x86-64.

/// @brief Compile-time fixed-size bitset backed by an array of 64-bit words.
///
/// Bits are packed into `ceil(N / 64)` `u64` words. The top word is masked to
/// prevent phantom bits beyond position `N - 1` from affecting `all()`, `count()`,
/// bitwise complement, and shift operations.
///
/// All single-bit operations and bitwise operators are O(1) or O(N/64).
/// `count()` delegates to `__builtin_popcountll` (maps to the POPCNT instruction
/// on x86-64 and equivalent on AArch64), with a portable fallback.
///
/// @tparam N Number of bits. Must be greater than zero.
template<usize N>
class BitSet {
    static_assert(N > 0, "BitSet size must be positive");
    static constexpr usize WORDS  = (N + 63) / 64;
    static constexpr u64   TOP_MASK = N % 64 == 0
                                    ? ~u64(0)
                                    : (u64(1) << (N % 64)) - 1;

    u64 words_[WORDS];

    void mask_top() noexcept { words_[WORDS - 1] &= TOP_MASK; }

public:
    /// @brief Default constructor. Initialises all bits to 0.
    /// @complexity O(N/64).
    constexpr BitSet() noexcept { mem::zero(words_, sizeof(words_)); }

    // ── Bit manipulation ──────────────────────────────────────────────────────

    /// @brief Sets bit @p i to 1.
    /// @pre `i < N`.
    /// @complexity O(1).
    void set(usize i) noexcept   { words_[i >> 6] |=  (u64(1) << (i & 63)); }

    /// @brief Clears bit @p i to 0.
    /// @pre `i < N`.
    /// @complexity O(1).
    void reset(usize i) noexcept { words_[i >> 6] &= ~(u64(1) << (i & 63)); }

    /// @brief Flips (toggles) bit @p i.
    /// @pre `i < N`.
    /// @complexity O(1).
    void flip(usize i) noexcept  { words_[i >> 6] ^=  (u64(1) << (i & 63)); }

    /// @brief Returns the value of bit @p i.
    /// @pre `i < N`.
    /// @return `true` if bit @p i is set.
    /// @complexity O(1).
    [[nodiscard]] bool test(usize i) const noexcept {
        return (words_[i >> 6] >> (i & 63)) & 1;
    }

    /// @brief Returns the value of bit @p i (alias for `test`).
    /// @pre `i < N`.
    /// @complexity O(1).
    [[nodiscard]] bool operator[](usize i) const noexcept { return test(i); }

    /// @brief Sets all N bits to 1.
    /// @complexity O(N/64).
    void set_all()   noexcept { for (usize i = 0; i < WORDS; ++i) words_[i] = ~u64(0); mask_top(); }

    /// @brief Clears all N bits to 0.
    /// @complexity O(N/64).
    void reset_all() noexcept { mem::zero(words_, sizeof(words_)); }

    /// @brief Flips all N bits.
    /// @complexity O(N/64).
    void flip_all()  noexcept { for (usize i = 0; i < WORDS; ++i) words_[i] ^= ~u64(0); mask_top(); }

    // ── Queries ───────────────────────────────────────────────────────────────

    /// @brief Returns `true` if all bits are 0.
    /// @complexity O(N/64).
    [[nodiscard]] bool none() const noexcept {
        for (usize i = 0; i < WORDS; ++i) if (words_[i]) return false;
        return true;
    }

    /// @brief Returns `true` if at least one bit is 1.
    /// @complexity O(N/64).
    [[nodiscard]] bool any() const noexcept { return !none(); }

    /// @brief Returns `true` if all N bits are 1.
    /// @complexity O(N/64).
    [[nodiscard]] bool all() const noexcept {
        for (usize i = 0; i + 1 < WORDS; ++i) if (words_[i] != ~u64(0)) return false;
        return words_[WORDS - 1] == TOP_MASK;
    }

    /// @brief Returns the number of bits set to 1.
    ///
    /// Uses `__builtin_popcountll` on GCC/Clang (maps to POPCNT on x86-64);
    /// falls back to Kernighan's bit-counting loop on other compilers.
    /// @complexity O(N/64).
    [[nodiscard]] usize count() const noexcept {
        usize c = 0;
#if defined(__GNUC__) || defined(__clang__)
        for (usize i = 0; i < WORDS; ++i) c += static_cast<usize>(__builtin_popcountll(words_[i]));
#else
        for (usize i = 0; i < WORDS; ++i) {
            u64 w = words_[i];
            while (w) { ++c; w &= w - 1; }
        }
#endif
        return c;
    }

    /// @brief Returns the compile-time bit count @p N.
    /// @complexity O(1).
    [[nodiscard]] constexpr usize size() const noexcept { return N; }

    /// @brief Returns the index of the lowest set bit, or @p N if no bit is set.
    ///
    /// Uses `__builtin_ctzll` on GCC/Clang (maps to BSF/TZCNT on x86-64).
    /// @complexity O(N/64) worst case.
    [[nodiscard]] usize lowest_set() const noexcept {
        for (usize i = 0; i < WORDS; ++i) {
            if (words_[i]) {
#if defined(__GNUC__) || defined(__clang__)
                return i * 64 + static_cast<usize>(__builtin_ctzll(words_[i]));
#else
                u64 w = words_[i]; usize b = 0;
                while (!(w & 1)) { w >>= 1; ++b; }
                return i * 64 + b;
#endif
            }
        }
        return N;
    }

    // ── Bitwise operators ─────────────────────────────────────────────────────

    /// @brief Returns the bitwise AND of this bitset and @p o.
    [[nodiscard]] BitSet operator&(const BitSet& o) const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = words_[i] & o.words_[i]; return r;
    }
    /// @brief Returns the bitwise OR of this bitset and @p o.
    [[nodiscard]] BitSet operator|(const BitSet& o) const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = words_[i] | o.words_[i]; return r;
    }
    /// @brief Returns the bitwise XOR of this bitset and @p o.
    [[nodiscard]] BitSet operator^(const BitSet& o) const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = words_[i] ^ o.words_[i]; return r;
    }
    /// @brief Returns the bitwise complement of this bitset.
    /// @note Bits beyond position `N - 1` are masked to 0 in the result.
    [[nodiscard]] BitSet operator~() const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = ~words_[i]; r.mask_top(); return r;
    }

    /// @brief In-place bitwise AND.
    BitSet& operator&=(const BitSet& o) noexcept { for (usize i=0;i<WORDS;++i) words_[i]&=o.words_[i]; return *this; }
    /// @brief In-place bitwise OR.
    BitSet& operator|=(const BitSet& o) noexcept { for (usize i=0;i<WORDS;++i) words_[i]|=o.words_[i]; return *this; }
    /// @brief In-place bitwise XOR.
    BitSet& operator^=(const BitSet& o) noexcept { for (usize i=0;i<WORDS;++i) words_[i]^=o.words_[i]; return *this; }

    /// @brief Returns this bitset left-shifted by @p shift positions.
    /// @note Bits shifted beyond position `N - 1` are lost.
    [[nodiscard]] BitSet operator<<(usize shift) const noexcept {
        BitSet r;
        if (shift >= N) return r;
        usize word_shift = shift >> 6, bit_shift = shift & 63;
        for (usize i = WORDS; i-- > word_shift;) {
            r.words_[i] = words_[i - word_shift] << bit_shift;
            if (bit_shift && i > word_shift)
                r.words_[i] |= words_[i - word_shift - 1] >> (64 - bit_shift);
        }
        r.mask_top(); return r;
    }

    /// @brief Returns this bitset right-shifted by @p shift positions.
    [[nodiscard]] BitSet operator>>(usize shift) const noexcept {
        BitSet r;
        if (shift >= N) return r;
        usize word_shift = shift >> 6, bit_shift = shift & 63;
        for (usize i = 0; i + word_shift < WORDS; ++i) {
            r.words_[i] = words_[i + word_shift] >> bit_shift;
            if (bit_shift && i + word_shift + 1 < WORDS)
                r.words_[i] |= words_[i + word_shift + 1] << (64 - bit_shift);
        }
        return r;
    }

    /// @brief Returns `true` if this bitset and @p o have identical bit patterns.
    [[nodiscard]] bool operator==(const BitSet& o) const noexcept {
        return mem::compare(words_, o.words_, sizeof(words_)) == 0;
    }
    /// @brief Returns `true` if this bitset and @p o differ.
    [[nodiscard]] bool operator!=(const BitSet& o) const noexcept { return !(*this == o); }
};

// ── DynBitSet — runtime-sized bitset ─────────────────────────────────────────

/// @brief Heap-allocated bitset with a runtime-specified bit count.
///
/// Behaves like `BitSet<N>` but the bit count is supplied at construction time.
/// The backing array of `u64` words is allocated on the heap and freed in the
/// destructor.
///
/// @note The top word is masked after `set_all()` and `flip()` operations to
///       prevent phantom bits beyond `size() - 1`.
class DynBitSet {
    u64*  words_;
    usize nbits_;
    usize nwords_;

    static usize words_for(usize n) noexcept { return (n + 63) / 64; }
    void mask_top() noexcept {
        usize rem = nbits_ & 63;
        if (rem) words_[nwords_ - 1] &= (u64(1) << rem) - 1;
    }

public:
    /// @brief Constructs a dynamic bitset with @p n bits.
    /// @param n    Number of bits.
    /// @param init If `true`, all bits are initialised to 1; otherwise to 0.
    /// @complexity O(n/64).
    explicit DynBitSet(usize n, bool init = false)
        : nbits_(n), nwords_(words_for(n))
    {
        words_ = mem::alloc_n<u64>(nwords_);
        mem::zero(words_, nwords_ * sizeof(u64));
        if (init) { for (usize i = 0; i < nwords_; ++i) words_[i] = ~u64(0); mask_top(); }
    }

    /// @brief Copy constructor. Deep-copies the bit pattern from @p o.
    /// @complexity O(n/64).
    DynBitSet(const DynBitSet& o) : nbits_(o.nbits_), nwords_(o.nwords_) {
        words_ = mem::alloc_n<u64>(nwords_);
        mem::copy(words_, o.words_, nwords_ * sizeof(u64));
    }

    /// @brief Move constructor. Transfers ownership from @p o.
    /// @param o Source bitset. Left empty after the operation.
    /// @complexity O(1).
    DynBitSet(DynBitSet&& o) noexcept : words_(o.words_), nbits_(o.nbits_), nwords_(o.nwords_) {
        o.words_ = nullptr; o.nbits_ = o.nwords_ = 0;
    }

    /// @brief Destructor. Frees the backing word array.
    /// @complexity O(1).
    ~DynBitSet() { if (words_) mem::dealloc_n(words_); }

    /// @brief Copy-assignment operator.
    /// @return Reference to `*this`.
    /// @complexity O(n/64).
    DynBitSet& operator=(const DynBitSet& o) {
        if (this != &o) { DynBitSet t(o); *this = traits::move(t); }
        return *this;
    }

    /// @brief Move-assignment operator.
    /// @return Reference to `*this`.
    /// @complexity O(1).
    DynBitSet& operator=(DynBitSet&& o) noexcept {
        if (this != &o) {
            mem::dealloc_n(words_);
            words_ = o.words_; nbits_ = o.nbits_; nwords_ = o.nwords_;
            o.words_ = nullptr;
        }
        return *this;
    }

    /// @brief Sets bit @p i to 1. @pre `i < size()`. @complexity O(1).
    void set(usize i) noexcept   { words_[i>>6] |=  (u64(1)<<(i&63)); }
    /// @brief Clears bit @p i to 0. @pre `i < size()`. @complexity O(1).
    void reset(usize i) noexcept { words_[i>>6] &= ~(u64(1)<<(i&63)); }
    /// @brief Flips bit @p i. @pre `i < size()`. @complexity O(1).
    void flip(usize i) noexcept  { words_[i>>6] ^=  (u64(1)<<(i&63)); }

    /// @brief Returns the value of bit @p i. @pre `i < size()`. @complexity O(1).
    [[nodiscard]] bool test(usize i) const noexcept { return (words_[i>>6]>>(i&63))&1; }
    /// @brief Returns the value of bit @p i (alias for `test`). @complexity O(1).
    [[nodiscard]] bool operator[](usize i) const noexcept { return test(i); }

    /// @brief Returns the number of set bits (popcount).
    ///
    /// Uses `__builtin_popcountll` on GCC/Clang.
    /// @complexity O(n/64).
    [[nodiscard]] usize count() const noexcept {
        usize c = 0;
#if defined(__GNUC__) || defined(__clang__)
        for (usize i = 0; i < nwords_; ++i) c += static_cast<usize>(__builtin_popcountll(words_[i]));
#else
        for (usize i = 0; i < nwords_; ++i) { u64 w=words_[i]; while(w){++c;w&=w-1;} }
#endif
        return c;
    }

    /// @brief Returns the total number of bits in this bitset.
    /// @complexity O(1).
    [[nodiscard]] usize size() const noexcept { return nbits_; }

    /// @brief Sets all bits to 1.
    /// @complexity O(n/64).
    void set_all()   noexcept { for (usize i=0;i<nwords_;++i) words_[i]=~u64(0); mask_top(); }

    /// @brief Clears all bits to 0.
    /// @complexity O(n/64).
    void reset_all() noexcept { mem::zero(words_, nwords_*sizeof(u64)); }
};

} // namespace dsc
