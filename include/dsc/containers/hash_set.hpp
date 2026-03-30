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
    struct Iterator {
        Slot* slots; usize cap; usize idx;
        void skip() noexcept { while (idx < cap && slots[idx].psl == EMPTY_PSL) ++idx; }
        const K& operator*()  const noexcept { return slots[idx].key(); }
        const K* operator->() const noexcept { return &slots[idx].key(); }
        Iterator& operator++() noexcept { ++idx; skip(); return *this; }
        Iterator  operator++(int) noexcept { auto t = *this; ++(*this); return t; }
        bool operator==(Iterator o) const noexcept { return idx == o.idx; }
        bool operator!=(Iterator o) const noexcept { return idx != o.idx; }
    };

    // ── Construction ─────────────────────────────────────────────────────────
    HashSet() : slots_(nullptr), cap_(0), size_(0) { init_slots(INITIAL_CAP); }

    HashSet(const HashSet& o) : HashSet() { for (const auto& k : o) insert(k); }

    HashSet(HashSet&& o) noexcept : slots_(o.slots_), cap_(o.cap_), size_(o.size_) {
        o.slots_ = nullptr; o.cap_ = o.size_ = 0;
    }

    ~HashSet() { destroy_all(); free_slots(); }

    HashSet& operator=(const HashSet& o) {
        if (this != &o) { HashSet t(o); *this = traits::move(t); }
        return *this;
    }
    HashSet& operator=(HashSet&& o) noexcept {
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

    // ── Modifiers ─────────────────────────────────────────────────────────────
    bool insert(const K& k) { return emplace_impl(k); }
    bool insert(K&& k)      { return emplace_impl(traits::move(k)); }

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

    void clear() noexcept { destroy_all(); init_slots(INITIAL_CAP); }

    // ── Lookup ────────────────────────────────────────────────────────────────
    [[nodiscard]] bool contains(const K& k) const noexcept {
        usize idx = find_slot(k);
        return idx < cap_ && slots_[idx].psl != EMPTY_PSL;
    }

    // ── Iterators ─────────────────────────────────────────────────────────────
    Iterator begin() noexcept { Iterator it{slots_, cap_, 0}; it.skip(); return it; }
    Iterator end()   noexcept { return {slots_, cap_, cap_}; }

    Iterator begin() const noexcept { Iterator it{slots_, cap_, 0}; it.skip(); return it; }
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
