#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"
#include "../core/optional.hpp"

namespace dsc {

// ── SkipList<K, V, Cmp> ───────────────────────────────────────────────────────
// Probabilistic sorted map using layered linked lists.
//
// A skip list maintains multiple levels of forward pointers.  Level 0 is a
// full sorted linked list; each higher level skips ~2× more nodes.  A random
// coin flip decides how many levels a new node gets.
//
// Expected complexities (with probability 1 - 1/n²):
//   insert / erase / find : O(log n)
//   Memory                : O(n log n) total pointers
//
// Advantages over balanced BSTs:
//   • Lock-free variants are simple (no rotations to synchronise).
//   • Cache-friendly sequential scan at level 0.
//   • Simpler implementation while achieving the same asymptotic bounds.
template<typename K, typename V,
         typename Cmp = decltype([](const K& a, const K& b){ return a < b; })>
class SkipList {
    static constexpr usize MAX_LEVELS = 32;
    static constexpr u32   PROB       = 0x40000000u;  // ~25 % chance per level (RAND_MAX/4)

    struct Node {
        K     key;
        V     val;
        usize levels;
        Node* forward[];  // flexible array; `levels` entries follow the struct

        static Node* create(const K& k, const V& v, usize lvl) {
            void* mem = ::operator new(sizeof(Node) + lvl * sizeof(Node*));
            Node* n = static_cast<Node*>(mem);
            new (&n->key) K(k);
            new (&n->val) V(v);
            n->levels = lvl;
            for (usize i = 0; i < lvl; ++i) n->forward[i] = nullptr;
            return n;
        }

        static void destroy(Node* n) noexcept {
            n->key.~K(); n->val.~V();
            ::operator delete(n);
        }
    };

    Node*  head_;        // sentinel header with MAX_LEVELS forward pointers
    usize  level_;       // current highest active level (0-based)
    usize  size_;
    u64    seed_;        // xorshift RNG state
    Cmp    cmp_{};

    // xorshift64 — no stdlib random needed
    u64 rng() noexcept {
        seed_ ^= seed_ << 13;
        seed_ ^= seed_ >> 7;
        seed_ ^= seed_ << 17;
        return seed_;
    }

    usize random_level() noexcept {
        usize lvl = 1;
        while (lvl < MAX_LEVELS && (rng() & 0xFFFFFFFFu) < PROB) ++lvl;
        return lvl;
    }

public:
    explicit SkipList(u64 seed = 0xdeadbeefcafe1337ULL) : level_(1), size_(0), seed_(seed) {
        void* mem = ::operator new(sizeof(Node) + MAX_LEVELS * sizeof(Node*));
        head_ = static_cast<Node*>(mem);
        head_->levels = MAX_LEVELS;
        for (usize i = 0; i < MAX_LEVELS; ++i) head_->forward[i] = nullptr;
    }

    ~SkipList() {
        Node* cur = head_->forward[0];
        while (cur) { Node* next = cur->forward[0]; Node::destroy(cur); cur = next; }
        ::operator delete(head_);
    }

    SkipList(const SkipList&)            = delete;
    SkipList& operator=(const SkipList&) = delete;

    // ── Insert ────────────────────────────────────────────────────────────────
    bool insert(const K& k, const V& v) {
        Node* update[MAX_LEVELS];
        Node* cur = head_;

        for (usize i = level_; i-- > 0;) {
            while (cur->forward[i] && cmp_(cur->forward[i]->key, k))
                cur = cur->forward[i];
            update[i] = cur;
        }

        Node* found = cur->forward[0];
        if (found && !cmp_(found->key, k) && !cmp_(k, found->key)) {
            found->val = v; return false;  // update existing
        }

        usize nlvl = random_level();
        if (nlvl > level_) {
            for (usize i = level_; i < nlvl; ++i) update[i] = head_;
            level_ = nlvl;
        }

        Node* n = Node::create(k, v, nlvl);
        for (usize i = 0; i < nlvl; ++i) {
            n->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = n;
        }
        ++size_;
        return true;
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    bool erase(const K& k) noexcept {
        Node* update[MAX_LEVELS];
        Node* cur = head_;

        for (usize i = level_; i-- > 0;) {
            while (cur->forward[i] && cmp_(cur->forward[i]->key, k))
                cur = cur->forward[i];
            update[i] = cur;
        }

        Node* target = cur->forward[0];
        if (!target || cmp_(k, target->key) || cmp_(target->key, k)) return false;

        for (usize i = 0; i < level_; ++i) {
            if (update[i]->forward[i] != target) break;
            update[i]->forward[i] = target->forward[i];
        }
        Node::destroy(target);
        while (level_ > 1 && !head_->forward[level_ - 1]) --level_;
        --size_;
        return true;
    }

    // ── Lookup ────────────────────────────────────────────────────────────────
    [[nodiscard]] Optional<V*> find(const K& k) noexcept {
        Node* cur = head_;
        for (usize i = level_; i-- > 0;)
            while (cur->forward[i] && cmp_(cur->forward[i]->key, k))
                cur = cur->forward[i];
        cur = cur->forward[0];
        if (cur && !cmp_(cur->key, k) && !cmp_(k, cur->key))
            return some(&cur->val);
        return none_of<V*>();
    }

    [[nodiscard]] bool contains(const K& k) noexcept {
        return find(k).has_value();
    }

    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Iterator (level-0 forward scan) ──────────────────────────────────────
    struct Iterator {
        Node* n;
        struct Pair { const K& key; V& val; };
        Pair operator*() noexcept { return {n->key, n->val}; }
        Iterator& operator++() noexcept { n = n->forward[0]; return *this; }
        Iterator  operator++(int) noexcept { auto t = *this; ++(*this); return t; }
        bool operator==(Iterator o) const noexcept { return n == o.n; }
        bool operator!=(Iterator o) const noexcept { return n != o.n; }
    };

    Iterator begin() noexcept { return {head_->forward[0]}; }
    Iterator end()   noexcept { return {nullptr}; }
};

} // namespace dsc
