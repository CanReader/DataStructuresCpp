#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/memory.hpp"
#include "../core/allocator.hpp"
#include "../core/hash.hpp"

namespace dsc {

// ── HashSet<K, H, Eq> ────────────────────────────────────────────────────────
// Robin Hood open-addressing hash set.
// Same design as HashMap; stores only keys (no associated value).
// See hash_map.hpp for algorithm rationale.

/// @brief Open-addressing hash set using Robin Hood hashing with backward-shift deletion.
///
/// Stores only keys (no associated values). Uses the same Robin Hood algorithm
/// as `HashMap`: each slot tracks a probe sequence length (PSL), incoming
/// inserts displace shorter-PSL residents, and deletions use backward-shift
/// rather than tombstones.
///
/// Capacity is always a power of two — modulo wrapping is a single bitmask.
/// Maximum load factor is 87.5%; a capacity-doubling rehash occurs automatically.
///
/// @tparam K  Key type. Must be hashable by @p H and equality-comparable by @p Eq.
/// @tparam H  Hash functor. Defaults to `Hash<K>` (FNV-1a + Fibonacci mixing).
/// @tparam Eq Equality functor. Defaults to `KeyEqual<K>` (operator==).
template<typename K,
         typename H  = Hash<K>,
         typename Eq = KeyEqual<K>>
class HashSet {
    static constexpr usize INITIAL_CAP = 8;
    static constexpr u8    EMPTY_PSL   = 0xFF;
    static constexpr f64   MAX_LOAD    = 0.875;

    struct Slot {
        alignas(K) unsigned char key_buf[sizeof(K)];
        u8 psl;
        K&       key()       noexcept { return *reinterpret_cast<K*>(key_buf); }
        const K& key() const noexcept { return *reinterpret_cast<const K*>(key_buf); }
    };

public:
    // ── Iterator ──────────────────────────────────────────────────────────────

    /// @brief Forward iterator over all keys in the set.
    ///
    /// Traverses the flat slot array, skipping empty slots. Iteration order is
    /// unspecified (depends on hash distribution). Invalidated by any insert or
    /// rehash operation.
    struct Iterator {
        Slot* slots; usize cap; usize idx;

        /// @brief Advances past empty slots.
        void skip() noexcept { while (idx < cap && slots[idx].psl == EMPTY_PSL) ++idx; }

        /// @brief Dereferences to a const reference to the current key.
        const K& operator*()  const noexcept { return slots[idx].key(); }

        /// @brief Arrow operator to the current key.
        const K* operator->() const noexcept { return &slots[idx].key(); }

        /// @brief Pre-increment. Moves to the next occupied slot.
        Iterator& operator++() noexcept { ++idx; skip(); return *this; }
        /// @brief Post-increment.
        Iterator  operator++(int) noexcept { auto t = *this; ++(*this); return t; }
        bool operator==(Iterator o) const noexcept { return idx == o.idx; }
        bool operator!=(Iterator o) const noexcept { return idx != o.idx; }
    };

    // ── Construction ─────────────────────────────────────────────────────────

    /// @brief Default constructor. Allocates an initial slot array of capacity 8.
    /// @complexity O(1).
    HashSet() : slots_(nullptr), cap_(0), size_(0) { init_slots(INITIAL_CAP); }

    /// @brief Copy constructor. Deep-copies all keys from @p o.
    /// @param o Source set to copy from.
    /// @complexity O(n).
    HashSet(const HashSet& o) : HashSet() { for (const auto& k : o) insert(k); }

    /// @brief Move constructor. Transfers ownership of the slot array from @p o.
    /// @param o Source set to move from. Left empty after the operation.
    /// @complexity O(1).
    HashSet(HashSet&& o) noexcept : slots_(o.slots_), cap_(o.cap_), size_(o.size_) {
        o.slots_ = nullptr; o.cap_ = o.size_ = 0;
    }

    /// @brief Destructor. Destroys all keys and releases the allocation.
    /// @complexity O(n).
    ~HashSet() { destroy_all(); free_slots(); }

    /// @brief Copy-assignment operator. Replaces contents with a copy of @p o.
    /// @return Reference to `*this`.
    /// @complexity O(n).
    HashSet& operator=(const HashSet& o) {
        if (this != &o) { HashSet t(o); *this = traits::move(t); }
        return *this;
    }

    /// @brief Move-assignment operator. Transfers ownership from @p o.
    /// @return Reference to `*this`.
    /// @complexity O(n) to destroy current keys; O(1) for the transfer.
    HashSet& operator=(HashSet&& o) noexcept {
        if (this != &o) {
            destroy_all(); free_slots();
            slots_ = o.slots_; cap_ = o.cap_; size_ = o.size_;
            o.slots_ = nullptr; o.cap_ = o.size_ = 0;
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────

    /// @brief Returns the number of keys currently stored in the set.
    /// @complexity O(1).
    [[nodiscard]] usize size()  const noexcept { return size_; }

    /// @brief Returns `true` if the set contains no keys.
    /// @complexity O(1).
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Modifiers ─────────────────────────────────────────────────────────────

    /// @brief Inserts @p k into the set if not already present.
    /// @return `true` if the key was newly inserted; `false` if it already existed.
    /// @note Triggers a capacity-doubling rehash when the load factor exceeds 87.5%.
    /// @complexity O(1) amortised.
    bool insert(const K& k) { return emplace_impl(k); }

    /// @overload Move-inserts the key.
    bool insert(K&& k)      { return emplace_impl(traits::move(k)); }

    /// @brief Removes @p k from the set.
    ///
    /// Uses backward-shift deletion: no tombstones are left behind, keeping
    /// subsequent lookups fast.
    /// @return `true` if the key was found and removed; `false` otherwise.
    /// @complexity O(1) amortised.
    bool erase(const K& k) noexcept {
        usize idx = find_slot(k);
        if (idx >= cap_ || slots_[idx].psl == EMPTY_PSL) return false;
        slots_[idx].key().~K();
        --size_;
        for (;;) {
            usize next = (idx + 1) & (cap_ - 1);
            if (slots_[next].psl == EMPTY_PSL || slots_[next].psl == 0) {
                slots_[idx].psl = EMPTY_PSL;
                break;
            }
            mem::copy(slots_[idx].key_buf, slots_[next].key_buf, sizeof(K));
            slots_[idx].psl = slots_[next].psl - 1;
            slots_[next].psl = EMPTY_PSL;
            idx = next;
        }
        return true;
    }

    /// @brief Removes all keys, resetting to initial capacity.
    /// @complexity O(n).
    void clear() noexcept { destroy_all(); init_slots(INITIAL_CAP); }

    // ── Lookup ────────────────────────────────────────────────────────────────

    /// @brief Returns `true` if @p k is present in the set.
    /// @complexity O(1) expected.
    [[nodiscard]] bool contains(const K& k) const noexcept {
        usize idx = find_slot(k);
        return idx < cap_ && slots_[idx].psl != EMPTY_PSL;
    }

    // ── Iterators ─────────────────────────────────────────────────────────────

    /// @brief Returns an iterator to the first key in the set.
    /// @complexity O(capacity) worst case to skip empty slots.
    Iterator begin() noexcept { Iterator it{slots_, cap_, 0}; it.skip(); return it; }
    /// @brief Returns a past-the-end iterator.
    Iterator end()   noexcept { return {slots_, cap_, cap_}; }

    /// @brief Returns a const iterator to the first key in the set.
    Iterator begin() const noexcept { Iterator it{slots_, cap_, 0}; it.skip(); return it; }
    /// @brief Returns a past-the-end const iterator.
    Iterator end()   const noexcept { return {slots_, cap_, cap_}; }

private:
    Slot* slots_;
    usize cap_;
    usize size_;
    H     hasher_{};
    Eq    eq_{};

    void init_slots(usize cap) {
        cap_ = cap; size_ = 0;
        slots_ = mem::alloc_n<Slot>(cap_);
        for (usize i = 0; i < cap_; ++i) slots_[i].psl = EMPTY_PSL;
    }

    void free_slots() noexcept { if (slots_) { mem::dealloc_n(slots_); slots_ = nullptr; } }

    void destroy_all() noexcept {
        for (usize i = 0; i < cap_; ++i) {
            if (slots_[i].psl != EMPTY_PSL) {
                if constexpr (!traits::is_trivially_destructible_v<K>) slots_[i].key().~K();
                slots_[i].psl = EMPTY_PSL;
            }
        }
        size_ = 0;
    }

    usize bucket(u64 h) const noexcept { return static_cast<usize>(h) & (cap_ - 1); }

    usize find_slot(const K& k) const noexcept {
        u64 h = hasher_(k); usize idx = bucket(h); u8 dist = 0;
        while (true) {
            if (slots_[idx].psl == EMPTY_PSL || dist > slots_[idx].psl) return idx;
            if (eq_(slots_[idx].key(), k)) return idx;
            idx = (idx + 1) & (cap_ - 1);
            ++dist;
        }
    }

    void rehash(usize nc) {
        Slot* old = slots_; usize oc = cap_;
        init_slots(nc);
        for (usize i = 0; i < oc; ++i) {
            if (old[i].psl != EMPTY_PSL) {
                emplace_impl(traits::move(old[i].key()));
                if constexpr (!traits::is_trivially_destructible_v<K>) old[i].key().~K();
            }
        }
        mem::dealloc_n(old);
    }

    template<typename KK>
    bool emplace_impl(KK&& k) {
        if (size_ + 1 > static_cast<usize>(cap_ * MAX_LOAD)) rehash(cap_ * 2);

        usize idx = bucket(hasher_(k));
        alignas(K) unsigned char tk[sizeof(K)];
        new (tk) K(traits::forward<KK>(k));
        u8 cur_psl = 0;

        while (true) {
            if (slots_[idx].psl == EMPTY_PSL) {
                mem::copy(slots_[idx].key_buf, tk, sizeof(K));
                slots_[idx].psl = cur_psl;
                ++size_; return true;
            }
            if (eq_(slots_[idx].key(), *reinterpret_cast<K*>(tk))) {
                reinterpret_cast<K*>(tk)->~K();
                return false;
            }
            if (slots_[idx].psl < cur_psl) {
                u8 tmp_psl = slots_[idx].psl;
                unsigned char bk[sizeof(K)];
                mem::copy(bk, slots_[idx].key_buf, sizeof(K));
                mem::copy(slots_[idx].key_buf, tk, sizeof(K));
                mem::copy(tk, bk, sizeof(K));
                slots_[idx].psl = cur_psl;
                cur_psl = tmp_psl;
            }
            idx = (idx + 1) & (cap_ - 1);
            ++cur_psl;
        }
    }
};

} // namespace dsc
