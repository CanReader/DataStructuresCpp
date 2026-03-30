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

/// @brief Prefix tree (trie) supporting 256 byte-valued character sets.
///
/// Each string is stored as a path from the root to a terminal node. Every
/// node can have up to 256 children (one per possible byte value), making
/// lookups purely index-based with no character comparisons.
///
/// A `count` field in each node tracks how many inserted strings pass through
/// it, enabling O(m) prefix-count queries. Empty nodes are pruned on `erase`.
///
/// Copy construction is disabled because deep-copying a 256-ary tree is
/// extremely expensive; use an explicit `clone()` pattern if needed.
///
/// @tparam CharT Character type. Defaults to `char`. Must be convertible to
///               `unsigned char` for index calculation.
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
    /// @brief Default constructor. Creates an empty trie with a single root node.
    /// @complexity O(1).
    Trie() : root_(new_node()), words_(0) {}

    /// @brief Destructor. Recursively destroys all nodes.
    /// @complexity O(n × ALPHA) in the worst case.
    ~Trie() { destroy(root_); }

    /// @brief Copy constructor is disabled — use an explicit clone pattern instead.
    Trie(const Trie&)            = delete;
    /// @brief Copy assignment is disabled.
    Trie& operator=(const Trie&) = delete;

    /// @brief Move constructor. Transfers ownership of the trie from @p o.
    /// @param o Source trie. Left empty after the operation.
    /// @complexity O(1).
    Trie(Trie&& o) noexcept : root_(o.root_), words_(o.words_) {
        o.root_ = nullptr; o.words_ = 0;
    }

    // ── Insert ────────────────────────────────────────────────────────────────

    /// @brief Inserts the string `str[0..len)` into the trie.
    ///
    /// Traverses (creating) nodes for each character and marks the final node
    /// as terminal. Also increments `count` along the path for prefix queries.
    /// @param str Pointer to the character data.
    /// @param len Length of the string.
    /// @complexity O(len).
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

    /// @brief Inserts a null-terminated string.
    /// @param cstr Null-terminated character string.
    /// @complexity O(len).
    void insert(const CharT* cstr) {
        usize len = 0; while (cstr[len]) ++len;
        insert(cstr, len);
    }

    // ── Exact lookup ──────────────────────────────────────────────────────────

    /// @brief Returns `true` if `str[0..len)` was previously inserted.
    /// @param str Pointer to the character data.
    /// @param len Length of the string.
    /// @complexity O(len).
    [[nodiscard]] bool contains(const CharT* str, usize len) const noexcept {
        const Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(str[i]);
            if (!cur->children[c]) return false;
            cur = cur->children[c];
        }
        return cur->terminal;
    }

    /// @brief Returns `true` if the null-terminated string @p cstr was previously inserted.
    /// @param cstr Null-terminated character string.
    /// @complexity O(len).
    [[nodiscard]] bool contains(const CharT* cstr) const noexcept {
        usize len = 0; while (cstr[len]) ++len;
        return contains(cstr, len);
    }

    // ── Prefix check ──────────────────────────────────────────────────────────

    /// @brief Returns `true` if any inserted string starts with `prefix[0..len)`.
    /// @param prefix Pointer to the prefix data.
    /// @param len    Length of the prefix.
    /// @complexity O(len).
    [[nodiscard]] bool starts_with(const CharT* prefix, usize len) const noexcept {
        const Node* cur = root_;
        for (usize i = 0; i < len; ++i) {
            u8 c = static_cast<u8>(prefix[i]);
            if (!cur->children[c]) return false;
            cur = cur->children[c];
        }
        return true;
    }

    /// @brief Returns the number of inserted strings that share the given prefix.
    ///
    /// Uses the `count` field maintained on each node during insertions.
    /// @param prefix Pointer to the prefix data.
    /// @param len    Length of the prefix.
    /// @return Number of inserted strings that begin with this prefix (0 if none).
    /// @complexity O(len).
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

    /// @brief Removes the string `str[0..len)` from the trie.
    ///
    /// Decrements `count` along the path and prunes leaf nodes that carry no
    /// remaining strings. Uses an iterative approach with a fixed-size path
    /// buffer (maximum string length: 4095 characters).
    ///
    /// @param str Pointer to the character data.
    /// @param len Length of the string. Must be < 4096.
    /// @return `true` if the string was found and removed; `false` otherwise.
    /// @complexity O(len).
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

    /// @brief Returns the total number of strings currently stored.
    /// @complexity O(1).
    [[nodiscard]] usize word_count() const noexcept { return words_; }

    /// @brief Returns `true` if no strings are stored.
    /// @complexity O(1).
    [[nodiscard]] bool  empty()      const noexcept { return words_ == 0; }

    /// @brief Removes all strings and resets to a single empty root node.
    /// @complexity O(n × ALPHA) in the worst case.
    void clear() noexcept {
        destroy(root_);
        root_  = new_node();
        words_ = 0;
    }
};

} // namespace dsc
