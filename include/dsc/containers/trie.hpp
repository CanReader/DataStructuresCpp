#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"
#include "../core/optional.hpp"
#include "array.hpp"

namespace dsc {

// ── Trie (prefix tree) ────────────────────────────────────────────────────────
// Stores strings as paths through a tree of character nodes.
// Supports 256 byte values (full ASCII + extended).
//
// Operations:
//   insert(str, len)      — O(m), m = string length
//   contains(str, len)    — O(m) exact match
//   starts_with(str, len) — O(m) prefix check
//   erase(str, len)       — O(m)
//   count_with_prefix     — O(m + k), k = matching keys
//
// Use PoolAllocator via the second template parameter to cut allocation
// overhead on workloads with many short strings.
template<typename CharT = char>
class Trie {
    static constexpr usize ALPHA = 256;

    struct Node {
        Node*  children[ALPHA];
        bool   terminal;
        usize  count;   // number of words passing through this node

        Node() noexcept : terminal(false), count(0) {
            for (usize i = 0; i < ALPHA; ++i) children[i] = nullptr;
        }
    };

    using NodeAlloc = Allocator<Node>;

    Node* root_;
    usize words_;

    Node* new_node() {
        Node* n = NodeAlloc::allocate(1);
        NodeAlloc::construct(n);
        return n;
    }

    void destroy(Node* n) noexcept {
        if (!n) return;
        for (usize i = 0; i < ALPHA; ++i) destroy(n->children[i]);
        NodeAlloc::destroy(n);
        NodeAlloc::deallocate(n);
    }

public:
    Trie() : root_(new_node()), words_(0) {}
    ~Trie() { destroy(root_); }

    Trie(const Trie&)            = delete;  // deep copy is expensive; use explicit clone()
    Trie& operator=(const Trie&) = delete;

    Trie(Trie&& o) noexcept : root_(o.root_), words_(o.words_) {
        o.root_ = nullptr; o.words_ = 0;
    }

    // ── Insert ────────────────────────────────────────────────────────────────
    void insert(const CharT* str, usize len) {
        Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(str[i]);
            if (!cur->children[c]) cur->children[c] = new_node();
            cur = cur->children[c];
            ++cur->count;
        }
        if (!cur->terminal) { cur->terminal = true; ++words_; }
    }

    void insert(const CharT* cstr) {
        usize len = 0; while (cstr[len]) ++len;
        insert(cstr, len);
    }

    // ── Exact lookup ──────────────────────────────────────────────────────────
    [[nodiscard]] bool contains(const CharT* str, usize len) const noexcept {
        const Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(str[i]);
            if (!cur->children[c]) return false;
            cur = cur->children[c];
        }
        return cur->terminal;
    }

    [[nodiscard]] bool contains(const CharT* cstr) const noexcept {
        usize len = 0; while (cstr[len]) ++len;
        return contains(cstr, len);
    }

    // ── Prefix check ──────────────────────────────────────────────────────────
    [[nodiscard]] bool starts_with(const CharT* prefix, usize len) const noexcept {
        const Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(prefix[i]);
            if (!cur->children[c]) return false;
            cur = cur->children[c];
        }
        return true;
    }

    // How many inserted strings share this prefix
    [[nodiscard]] usize count_with_prefix(const CharT* prefix, usize len) const noexcept {
        const Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(prefix[i]);
            if (!cur->children[c]) return 0;
            cur = cur->children[c];
        }
        return cur->count;
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    bool erase(const CharT* str, usize len) noexcept {
        // Iterative with backtrack stack
        Node* path[4096];
        u8    chars[4096];
        if (len >= 4096) return false;

        Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(str[i]);
            if (!cur->children[c]) return false;
            path[i]  = cur;
            chars[i] = c;
            cur = cur->children[c];
        }
        if (!cur->terminal) return false;
        cur->terminal = false;
        --words_;

        // Decrement counts and prune empty nodes bottom-up
        for (usize i = len; i-- > 0;) {
            Node* child = path[i]->children[chars[i]];
            --child->count;
            if (child->count == 0 && !child->terminal) {
                NodeAlloc::destroy(child);
                NodeAlloc::deallocate(child);
                path[i]->children[chars[i]] = nullptr;
            }
        }
        return true;
    }

    [[nodiscard]] usize word_count() const noexcept { return words_; }
    [[nodiscard]] bool  empty()      const noexcept { return words_ == 0; }

    void clear() noexcept {
        destroy(root_);
        root_  = new_node();
        words_ = 0;
    }
};

} // namespace dsc
