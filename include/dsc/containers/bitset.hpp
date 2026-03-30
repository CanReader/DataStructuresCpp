#pragma once

#include "../core/primitives.hpp"
#include "../core/memory.hpp"

namespace dsc {

// ── BitSet<N> ─────────────────────────────────────────────────────────────────
// Compile-time fixed-size bitset stored in an array of u64 words.
// Every operation except count() is O(1).
// count() uses __builtin_popcountll — compiles to POPCNT on x86-64.
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
    constexpr BitSet() noexcept { mem::zero(words_, sizeof(words_)); }

    // ── Bit manipulation ──────────────────────────────────────────────────────
    void set(usize i) noexcept   { words_[i >> 6] |=  (u64(1) << (i & 63)); }
    void reset(usize i) noexcept { words_[i >> 6] &= ~(u64(1) << (i & 63)); }
    void flip(usize i) noexcept  { words_[i >> 6] ^=  (u64(1) << (i & 63)); }

    [[nodiscard]] bool test(usize i) const noexcept {
        return (words_[i >> 6] >> (i & 63)) & 1;
    }

    [[nodiscard]] bool operator[](usize i) const noexcept { return test(i); }

    void set_all()   noexcept { for (usize i = 0; i < WORDS; ++i) words_[i] = ~u64(0); mask_top(); }
    void reset_all() noexcept { mem::zero(words_, sizeof(words_)); }
    void flip_all()  noexcept { for (usize i = 0; i < WORDS; ++i) words_[i] ^= ~u64(0); mask_top(); }

    // ── Queries ───────────────────────────────────────────────────────────────
    [[nodiscard]] bool none() const noexcept {
        for (usize i = 0; i < WORDS; ++i) if (words_[i]) return false;
        return true;
    }

    [[nodiscard]] bool any() const noexcept { return !none(); }

    [[nodiscard]] bool all() const noexcept {
        for (usize i = 0; i + 1 < WORDS; ++i) if (words_[i] != ~u64(0)) return false;
        return words_[WORDS - 1] == TOP_MASK;
    }

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

    [[nodiscard]] constexpr usize size() const noexcept { return N; }

    // Index of the lowest set bit, or N if none
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
    BitSet operator&(const BitSet& o) const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = words_[i] & o.words_[i]; return r;
    }
    BitSet operator|(const BitSet& o) const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = words_[i] | o.words_[i]; return r;
    }
    BitSet operator^(const BitSet& o) const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = words_[i] ^ o.words_[i]; return r;
    }
    BitSet operator~() const noexcept {
        BitSet r; for (usize i = 0; i < WORDS; ++i) r.words_[i] = ~words_[i]; r.mask_top(); return r;
    }

    BitSet& operator&=(const BitSet& o) noexcept { for (usize i=0;i<WORDS;++i) words_[i]&=o.words_[i]; return *this; }
    BitSet& operator|=(const BitSet& o) noexcept { for (usize i=0;i<WORDS;++i) words_[i]|=o.words_[i]; return *this; }
    BitSet& operator^=(const BitSet& o) noexcept { for (usize i=0;i<WORDS;++i) words_[i]^=o.words_[i]; return *this; }

    BitSet operator<<(usize shift) const noexcept {
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

    BitSet operator>>(usize shift) const noexcept {
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

    bool operator==(const BitSet& o) const noexcept {
        return mem::compare(words_, o.words_, sizeof(words_)) == 0;
    }
    bool operator!=(const BitSet& o) const noexcept { return !(*this == o); }
};

// ── DynBitSet — runtime-sized bitset ─────────────────────────────────────────
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
    explicit DynBitSet(usize n, bool init = false)
        : nbits_(n), nwords_(words_for(n))
    {
        words_ = mem::alloc_n<u64>(nwords_);
        mem::zero(words_, nwords_ * sizeof(u64));
        if (init) { for (usize i = 0; i < nwords_; ++i) words_[i] = ~u64(0); mask_top(); }
    }

    DynBitSet(const DynBitSet& o) : nbits_(o.nbits_), nwords_(o.nwords_) {
        words_ = mem::alloc_n<u64>(nwords_);
        mem::copy(words_, o.words_, nwords_ * sizeof(u64));
    }

    DynBitSet(DynBitSet&& o) noexcept : words_(o.words_), nbits_(o.nbits_), nwords_(o.nwords_) {
        o.words_ = nullptr; o.nbits_ = o.nwords_ = 0;
    }

    ~DynBitSet() { if (words_) mem::dealloc_n(words_); }

    DynBitSet& operator=(const DynBitSet& o) {
        if (this != &o) { DynBitSet t(o); *this = traits::move(t); }
        return *this;
    }
    DynBitSet& operator=(DynBitSet&& o) noexcept {
        if (this != &o) {
            mem::dealloc_n(words_);
            words_ = o.words_; nbits_ = o.nbits_; nwords_ = o.nwords_;
            o.words_ = nullptr;
        }
        return *this;
    }

    void set(usize i) noexcept   { words_[i>>6] |=  (u64(1)<<(i&63)); }
    void reset(usize i) noexcept { words_[i>>6] &= ~(u64(1)<<(i&63)); }
    void flip(usize i) noexcept  { words_[i>>6] ^=  (u64(1)<<(i&63)); }

    [[nodiscard]] bool test(usize i) const noexcept { return (words_[i>>6]>>(i&63))&1; }
    [[nodiscard]] bool operator[](usize i) const noexcept { return test(i); }

    [[nodiscard]] usize count() const noexcept {
        usize c = 0;
#if defined(__GNUC__) || defined(__clang__)
        for (usize i = 0; i < nwords_; ++i) c += static_cast<usize>(__builtin_popcountll(words_[i]));
#else
        for (usize i = 0; i < nwords_; ++i) { u64 w=words_[i]; while(w){++c;w&=w-1;} }
#endif
        return c;
    }

    [[nodiscard]] usize size() const noexcept { return nbits_; }

    void set_all()   noexcept { for (usize i=0;i<nwords_;++i) words_[i]=~u64(0); mask_top(); }
    void reset_all() noexcept { mem::zero(words_, nwords_*sizeof(u64)); }
};

} // namespace dsc
