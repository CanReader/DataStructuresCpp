#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/memory.hpp"
#include "../core/allocator.hpp"
#include "../core/optional.hpp"
#include "../core/hash.hpp"

namespace dsc {

// ── HashMap<K, V, H, Eq> ─────────────────────────────────────────────────────
// Open-addressing hash map using Robin Hood hashing.
//
// Why Robin Hood?
//   • Each slot tracks its probe distance (PSL = probe sequence length).
//   • On insert, if a new element has a longer PSL than the slot it hits,
//     they swap — poorer elements "steal" from richer ones.
//   • This equalises PSLs across all elements, reducing worst-case lookup
//     from O(n) to O(log n) expected and practically < 5 probes at 87.5 % load.
//   • Deletion uses "backward shift" — no tombstones, no load-factor bloat
//     from accumulated deletes, and lookup stays fast after many deletions.
//   • Power-of-2 bucket count → modulo is a cheap bitmask.
//
// Complexity (expected):
//   insert / erase / find : O(1) amortized
//   Memory overhead       : ~1.14× at 87.5 % load (best-in-class open addressing)
template<typename K, typename V,
         typename H  = Hash<K>,
         typename Eq = KeyEqual<K>>
class HashMap {
    static constexpr usize INITIAL_CAP  = 8;
    static constexpr u8    EMPTY_PSL    = 0xFF;   // sentinel: slot is empty
    static constexpr f64   MAX_LOAD     = 0.875;

    struct Slot {
        alignas(K) unsigned char key_buf[sizeof(K)];
        alignas(V) unsigned char val_buf[sizeof(V)];
        u8 psl;  // probe sequence length; EMPTY_PSL = empty

        K&       key()       noexcept { return *reinterpret_cast<K*>(key_buf); }
        const K& key() const noexcept { return *reinterpret_cast<const K*>(key_buf); }
        V&       val()       noexcept { return *reinterpret_cast<V*>(val_buf); }
        const V& val() const noexcept { return *reinterpret_cast<const V*>(val_buf); }
    };

public:
    // ── Pair ──────────────────────────────────────────────────────────────────
    struct Pair {
        const K& key;
        V&       value;
    };

    struct ConstPair {
        const K& key;
        const V& value;
    };

    // ── Iterator ──────────────────────────────────────────────────────────────
    struct Iterator {
        Slot* slots;
        usize cap;
        usize idx;

        void advance_to_filled() noexcept {
            while (idx < cap && slots[idx].psl == EMPTY_PSL) ++idx;
        }

        Pair operator*() noexcept { return {slots[idx].key(), slots[idx].val()}; }

        Iterator& operator++() noexcept { ++idx; advance_to_filled(); return *this; }
        Iterator  operator++(int) noexcept { auto t = *this; ++(*this); return t; }
        bool operator==(Iterator o) const noexcept { return idx == o.idx; }
        bool operator!=(Iterator o) const noexcept { return idx != o.idx; }
    };

    struct ConstIterator {
        const Slot* slots;
        usize cap;
        usize idx;

        void advance_to_filled() noexcept {
            while (idx < cap && slots[idx].psl == EMPTY_PSL) ++idx;
        }

        ConstPair operator*() const noexcept { return {slots[idx].key(), slots[idx].val()}; }

        ConstIterator& operator++() noexcept { ++idx; advance_to_filled(); return *this; }
        ConstIterator  operator++(int) noexcept { auto t = *this; ++(*this); return t; }
        bool operator==(ConstIterator o) const noexcept { return idx == o.idx; }
        bool operator!=(ConstIterator o) const noexcept { return idx != o.idx; }
    };

    // ── Construction ─────────────────────────────────────────────────────────
    HashMap() : slots_(nullptr), cap_(0), size_(0) { init_slots(INITIAL_CAP); }

    HashMap(const HashMap& o) : slots_(nullptr), cap_(0), size_(0) {
        init_slots(o.cap_);
        for (auto [k, v] : o) insert(k, v);
    }

    HashMap(HashMap&& o) noexcept : slots_(o.slots_), cap_(o.cap_), size_(o.size_) {
        o.slots_ = nullptr; o.cap_ = o.size_ = 0;
    }

    ~HashMap() { destroy_all(); free_slots(); }

    HashMap& operator=(const HashMap& o) {
        if (this != &o) { HashMap tmp(o); *this = traits::move(tmp); }
        return *this;
    }

    HashMap& operator=(HashMap&& o) noexcept {
        if (this != &o) {
            destroy_all(); free_slots();
            slots_ = o.slots_; cap_ = o.cap_; size_ = o.size_;
            o.slots_ = nullptr; o.cap_ = o.size_ = 0;
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Insert / update ───────────────────────────────────────────────────────
    // Returns true if a new element was inserted; false if key already existed.
    bool insert(const K& k, const V& v) { return emplace_impl(k, v); }
    bool insert(const K& k, V&& v)      { return emplace_impl(k, traits::move(v)); }
    bool insert(K&& k,      const V& v) { return emplace_impl(traits::move(k), v); }
    bool insert(K&& k,      V&& v)      { return emplace_impl(traits::move(k), traits::move(v)); }

    // Insert-or-assign: always writes the value, returns true if newly inserted.
    bool insert_or_assign(const K& k, V v) {
        usize idx = find_slot(k);
        if (idx < cap_ && slots_[idx].psl != EMPTY_PSL) {
            slots_[idx].val() = traits::move(v);
            return false;
        }
        return emplace_impl(k, traits::move(v));
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    bool erase(const K& k) noexcept {
        usize idx = find_slot(k);
        if (idx >= cap_ || slots_[idx].psl == EMPTY_PSL) return false;

        slots_[idx].key().~K();
        slots_[idx].val().~V();
        --size_;

        // Backward-shift deletion: pull neighbours back until PSL == 0 or empty
        for (;;) {
            usize next = (idx + 1) & (cap_ - 1);
            if (slots_[next].psl == EMPTY_PSL || slots_[next].psl == 0) {
                slots_[idx].psl = EMPTY_PSL;
                break;
            }
            mem::copy(slots_[idx].key_buf, slots_[next].key_buf, sizeof(K));
            mem::copy(slots_[idx].val_buf, slots_[next].val_buf, sizeof(V));
            slots_[idx].psl = slots_[next].psl - 1;
            slots_[next].psl = EMPTY_PSL;
            idx = next;
        }
        return true;
    }

    // ── Lookup ────────────────────────────────────────────────────────────────
    [[nodiscard]] Optional<V*> find(const K& k) noexcept {
        usize idx = find_slot(k);
        if (idx < cap_ && slots_[idx].psl != EMPTY_PSL)
            return some(&slots_[idx].val());
        return none_of<V*>();
    }

    [[nodiscard]] Optional<const V*> find(const K& k) const noexcept {
        usize idx = find_slot(k);
        if (idx < cap_ && slots_[idx].psl != EMPTY_PSL)
            return some<const V*>(&slots_[idx].val());
        return none_of<const V*>();
    }

    [[nodiscard]] bool contains(const K& k) const noexcept {
        usize idx = find_slot(k);
        return idx < cap_ && slots_[idx].psl != EMPTY_PSL;
    }

    // operator[] creates default-constructed V if key absent
    V& operator[](const K& k) {
        usize idx = find_slot(k);
        if (idx < cap_ && slots_[idx].psl != EMPTY_PSL) return slots_[idx].val();
        emplace_impl(k, V{});
        return slots_[find_slot(k)].val();
    }

    void clear() noexcept { destroy_all(); init_slots(INITIAL_CAP); }

    // ── Iterators ─────────────────────────────────────────────────────────────
    Iterator begin() noexcept {
        Iterator it{slots_, cap_, 0};
        it.advance_to_filled();
        return it;
    }
    Iterator end() noexcept { return {slots_, cap_, cap_}; }

    ConstIterator begin() const noexcept {
        ConstIterator it{slots_, cap_, 0};
        it.advance_to_filled();
        return it;
    }
    ConstIterator end() const noexcept { return {slots_, cap_, cap_}; }

private:
    Slot* slots_;
    usize cap_;
    usize size_;

    H  hasher_{};
    Eq eq_{};

    void init_slots(usize cap) {
        cap_   = cap;
        size_  = 0;
        slots_ = mem::alloc_n<Slot>(cap_);
        for (usize i = 0; i < cap_; ++i) slots_[i].psl = EMPTY_PSL;
    }

    void free_slots() noexcept {
        if (slots_) { mem::dealloc_n(slots_); slots_ = nullptr; }
    }

    void destroy_all() noexcept {
        for (usize i = 0; i < cap_; ++i) {
            if (slots_[i].psl != EMPTY_PSL) {
                if constexpr (!traits::is_trivially_destructible_v<K>) slots_[i].key().~K();
                if constexpr (!traits::is_trivially_destructible_v<V>) slots_[i].val().~V();
                slots_[i].psl = EMPTY_PSL;
            }
        }
        size_ = 0;
    }

    usize bucket(u64 h) const noexcept { return static_cast<usize>(h) & (cap_ - 1); }

    // Returns the index of the slot holding k, or an empty slot if not found.
    usize find_slot(const K& k) const noexcept {
        u64   h    = hasher_(k);
        usize idx  = bucket(h);
        u8    dist = 0;
        while (true) {
            if (slots_[idx].psl == EMPTY_PSL || dist > slots_[idx].psl) return idx;
            if (eq_(slots_[idx].key(), k)) return idx;
            idx = (idx + 1) & (cap_ - 1);
            ++dist;
        }
    }

    void rehash(usize new_cap) {
        Slot* old    = slots_;
        usize old_cap = cap_;
        init_slots(new_cap);
        for (usize i = 0; i < old_cap; ++i) {
            if (old[i].psl != EMPTY_PSL) {
                emplace_slot(traits::move(old[i].key()), traits::move(old[i].val()));
                if constexpr (!traits::is_trivially_destructible_v<K>) old[i].key().~K();
                if constexpr (!traits::is_trivially_destructible_v<V>) old[i].val().~V();
            }
        }
        mem::dealloc_n(old);
    }

    template<typename KK, typename VV>
    bool emplace_impl(KK&& k, VV&& v) {
        if (size_ + 1 > static_cast<usize>(cap_ * MAX_LOAD)) rehash(cap_ * 2);

        usize idx = bucket(hasher_(k));
        u8    psl = 0;

        // Temporary slot for Robin Hood displacement
        alignas(K) unsigned char tk[sizeof(K)];
        alignas(V) unsigned char tv[sizeof(V)];
        new (tk) K(traits::forward<KK>(k));
        new (tv) V(traits::forward<VV>(v));
        u8 cur_psl = 0;

        while (true) {
            if (slots_[idx].psl == EMPTY_PSL) {
                // Empty slot — place here
                mem::copy(slots_[idx].key_buf, tk, sizeof(K));
                mem::copy(slots_[idx].val_buf, tv, sizeof(V));
                slots_[idx].psl = cur_psl;
                ++size_;
                return true;
            }
            if (eq_(slots_[idx].key(), *reinterpret_cast<K*>(tk))) {
                // Key exists
                reinterpret_cast<K*>(tk)->~K();
                reinterpret_cast<V*>(tv)->~V();
                return false;
            }
            if (slots_[idx].psl < cur_psl) {
                // Robin Hood: steal this slot
                u8 tmp_psl = slots_[idx].psl;
                // Swap buffers
                unsigned char bk[sizeof(K)], bv[sizeof(V)];
                mem::copy(bk, slots_[idx].key_buf, sizeof(K));
                mem::copy(bv, slots_[idx].val_buf, sizeof(V));
                mem::copy(slots_[idx].key_buf, tk, sizeof(K));
                mem::copy(slots_[idx].val_buf, tv, sizeof(V));
                mem::copy(tk, bk, sizeof(K));
                mem::copy(tv, bv, sizeof(V));
                slots_[idx].psl = cur_psl;
                cur_psl = tmp_psl;
            }
            idx = (idx + 1) & (cap_ - 1);
            ++cur_psl;
        }
        (void)psl;
    }

    // Used during rehash — keys/values are already constructed
    void emplace_slot(K&& k, V&& v) {
        emplace_impl(traits::move(k), traits::move(v));
    }
};

} // namespace dsc
