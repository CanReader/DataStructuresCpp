#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"
#include "../core/optional.hpp"
#include "../core/iterator.hpp"

namespace dsc {

// ── TreeMap<K, V, Cmp> ───────────────────────────────────────────────────────
// Ordered key-value map backed by an AVL tree.
//
// AVL vs Red-Black:
//   • AVL trees are more strictly balanced (height ≤ 1.44 log₂ n vs RB's 2 log₂ n).
//   • This makes lookups ~25 % faster on average — preferred for read-heavy workloads.
//   • Insertions/deletions cost slightly more rotations but are still O(log n).
//
// Default comparator uses operator<.
template<typename K, typename V,
         typename Cmp = decltype([](const K& a, const K& b){ return a < b; })>
class TreeMap {
    struct Node {
        K     key;
        V     val;
        Node* left   = nullptr;
        Node* right  = nullptr;
        Node* parent = nullptr;
        i32   height = 1;

        template<typename KK, typename VV>
        Node(KK&& k, VV&& v)
            : key(traits::forward<KK>(k)), val(traits::forward<VV>(v)) {}
    };

    using NodeAlloc = Allocator<Node>;

    // ── Rotation / balance helpers ────────────────────────────────────────────
    static i32 node_height(const Node* n) noexcept { return n ? n->height : 0; }

    static i32 balance_factor(const Node* n) noexcept {
        return node_height(n->left) - node_height(n->right);
    }

    static void update_height(Node* n) noexcept {
        i32 lh = node_height(n->left), rh = node_height(n->right);
        n->height = 1 + (lh > rh ? lh : rh);
    }

    Node* rotate_right(Node* y) noexcept {
        Node* x  = y->left;
        Node* T2 = x->right;

        x->right = y; y->left = T2;
        x->parent = y->parent; y->parent = x;
        if (T2) T2->parent = y;

        if (x->parent) {
            if (x->parent->left  == y) x->parent->left  = x;
            else                       x->parent->right = x;
        } else root_ = x;

        update_height(y);
        update_height(x);
        return x;
    }

    Node* rotate_left(Node* x) noexcept {
        Node* y  = x->right;
        Node* T2 = y->left;

        y->left = x; x->right = T2;
        y->parent = x->parent; x->parent = y;
        if (T2) T2->parent = x;

        if (y->parent) {
            if (y->parent->left  == x) y->parent->left  = y;
            else                       y->parent->right = y;
        } else root_ = y;

        update_height(x);
        update_height(y);
        return y;
    }

    Node* rebalance(Node* n) noexcept {
        update_height(n);
        i32 bf = balance_factor(n);

        if (bf > 1) {
            if (balance_factor(n->left) < 0) rotate_left(n->left);
            return rotate_right(n);
        }
        if (bf < -1) {
            if (balance_factor(n->right) > 0) rotate_right(n->right);
            return rotate_left(n);
        }
        return n;
    }

    void rebalance_up(Node* n) noexcept {
        while (n) {
            Node* p = n->parent;
            rebalance(n);
            n = p;
        }
    }

    // Leftmost node in subtree
    static Node* leftmost(Node* n) noexcept {
        while (n && n->left) n = n->left;
        return n;
    }

    // Rightmost node in subtree
    static Node* rightmost(Node* n) noexcept {
        while (n && n->right) n = n->right;
        return n;
    }

    // In-order successor
    static Node* successor(Node* n) noexcept {
        if (n->right) return leftmost(n->right);
        Node* p = n->parent;
        while (p && n == p->right) { n = p; p = p->parent; }
        return p;
    }

    // In-order predecessor
    static Node* predecessor(Node* n) noexcept {
        if (n->left) return rightmost(n->left);
        Node* p = n->parent;
        while (p && n == p->left) { n = p; p = p->parent; }
        return p;
    }

public:
    // ── Pair ──────────────────────────────────────────────────────────────────
    struct Pair { const K& key; V& value; };

    // ── Iterator (in-order) ───────────────────────────────────────────────────
    struct Iterator : BidirectionalIterator<Iterator, Pair> {
        Node* n;
        mutable Pair cur_{*reinterpret_cast<K*>(0), *reinterpret_cast<V*>(0)};

        explicit Iterator(Node* n) noexcept : n(n) {}
        Iterator& increment() noexcept { n = successor(n);   return *this; }
        Iterator& decrement() noexcept { n = predecessor(n); return *this; }
        Pair& deref() const noexcept {
            new (&cur_) Pair{n->key, n->val};
            return cur_;
        }
        bool equal(Iterator o) const noexcept { return n == o.n; }
    };

    struct ConstIterator : BidirectionalIterator<ConstIterator, const Pair> {
        const Node* n;
        mutable Pair cur_{*reinterpret_cast<K*>(0), *reinterpret_cast<V*>(0)};

        explicit ConstIterator(const Node* n) noexcept : n(n) {}
        ConstIterator& increment() noexcept { n = successor(const_cast<Node*>(n)); return *this; }
        ConstIterator& decrement() noexcept { n = predecessor(const_cast<Node*>(n)); return *this; }
        const Pair& deref() const noexcept {
            new (&cur_) Pair{n->key, const_cast<V&>(n->val)};
            return cur_;
        }
        bool equal(ConstIterator o) const noexcept { return n == o.n; }
    };

    using iterator       = Iterator;
    using const_iterator = ConstIterator;

    // ── Construction ─────────────────────────────────────────────────────────
    TreeMap() : root_(nullptr), size_(0) {}

    TreeMap(const TreeMap& o) : root_(nullptr), size_(0) {
        for (auto [k, v] : o) insert(k, v);
    }

    TreeMap(TreeMap&& o) noexcept : root_(o.root_), size_(o.size_) {
        o.root_ = nullptr; o.size_ = 0;
    }

    ~TreeMap() { destroy_subtree(root_); }

    TreeMap& operator=(const TreeMap& o) {
        if (this != &o) { TreeMap t(o); *this = traits::move(t); }
        return *this;
    }
    TreeMap& operator=(TreeMap&& o) noexcept {
        if (this != &o) {
            destroy_subtree(root_);
            root_ = o.root_; size_ = o.size_;
            o.root_ = nullptr; o.size_ = 0;
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Insert ────────────────────────────────────────────────────────────────
    // Returns true if newly inserted, false if key already existed.
    bool insert(const K& k, const V& v) { return insert_impl(k, v); }
    bool insert(const K& k, V&& v)      { return insert_impl(k, traits::move(v)); }
    bool insert(K&& k, V&& v)           { return insert_impl(traits::move(k), traits::move(v)); }

    bool insert_or_assign(const K& k, V v) {
        Node* n = find_node(k);
        if (n) { n->val = traits::move(v); return false; }
        return insert_impl(k, traits::move(v));
    }

    // operator[] — creates default V if absent
    V& operator[](const K& k) {
        Node* n = find_node(k);
        if (n) return n->val;
        insert_impl(k, V{});
        return find_node(k)->val;
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    bool erase(const K& k) noexcept {
        Node* n = find_node(k);
        if (!n) return false;
        erase_node(n);
        return true;
    }

    // ── Lookup ────────────────────────────────────────────────────────────────
    [[nodiscard]] Optional<V*> find(const K& k) noexcept {
        Node* n = find_node(k);
        return n ? some(&n->val) : none_of<V*>();
    }
    [[nodiscard]] Optional<const V*> find(const K& k) const noexcept {
        const Node* n = find_node(k);
        return n ? some<const V*>(&n->val) : none_of<const V*>();
    }
    [[nodiscard]] bool contains(const K& k) const noexcept { return find_node(k) != nullptr; }

    // Min/max (O(log n))
    [[nodiscard]] Optional<K*> min_key() noexcept {
        if (!root_) return none_of<K*>();
        return some(&leftmost(root_)->key);
    }
    [[nodiscard]] Optional<K*> max_key() noexcept {
        if (!root_) return none_of<K*>();
        return some(&rightmost(root_)->key);
    }

    void clear() noexcept { destroy_subtree(root_); root_ = nullptr; size_ = 0; }

    // ── Iterators ─────────────────────────────────────────────────────────────
    Iterator begin() noexcept       { return Iterator(leftmost(root_)); }
    Iterator end()   noexcept       { return Iterator(nullptr); }
    ConstIterator begin() const noexcept { return ConstIterator(leftmost(root_)); }
    ConstIterator end()   const noexcept { return ConstIterator(nullptr); }

private:
    Node* root_;
    usize size_;
    Cmp   cmp_{};

    Node* find_node(const K& k) const noexcept {
        Node* cur = root_;
        while (cur) {
            if (cmp_(k, cur->key))       cur = cur->left;
            else if (cmp_(cur->key, k))  cur = cur->right;
            else                         return cur;
        }
        return nullptr;
    }

    template<typename KK, typename VV>
    bool insert_impl(KK&& k, VV&& v) {
        Node* parent = nullptr;
        Node** link  = &root_;

        while (*link) {
            parent = *link;
            if (cmp_(k, parent->key))      link = &parent->left;
            else if (cmp_(parent->key, k)) link = &parent->right;
            else return false;  // duplicate
        }

        Node* n = NodeAlloc::allocate(1);
        new (n) Node(traits::forward<KK>(k), traits::forward<VV>(v));
        n->parent = parent;
        *link = n;
        ++size_;
        rebalance_up(parent);
        return true;
    }

    void erase_node(Node* n) noexcept {
        if (n->left && n->right) {
            // Replace with in-order successor
            Node* s = leftmost(n->right);
            // Swap contents (not pointers)
            traits::swap(const_cast<K&>(n->key), const_cast<K&>(s->key));
            traits::swap(n->val, s->val);
            n = s;
        }

        Node* child  = n->left ? n->left : n->right;
        Node* parent = n->parent;

        if (child) child->parent = parent;

        if (parent) {
            if (parent->left == n) parent->left  = child;
            else                   parent->right = child;
        } else {
            root_ = child;
        }

        Node* rebal = parent;
        n->~Node();
        NodeAlloc::deallocate(n);
        --size_;
        rebalance_up(rebal);
    }

    void destroy_subtree(Node* n) noexcept {
        if (!n) return;
        destroy_subtree(n->left);
        destroy_subtree(n->right);
        n->~Node();
        NodeAlloc::deallocate(n);
    }
};

} // namespace dsc
