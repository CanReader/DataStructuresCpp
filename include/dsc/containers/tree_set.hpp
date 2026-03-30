#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"
#include "../core/optional.hpp"
#include "../core/iterator.hpp"

namespace dsc {

// ── TreeSet<K, Cmp> ──────────────────────────────────────────────────────────
// Ordered set backed by an AVL tree.
// AVL provides stricter balance than Red-Black → faster average lookups.
// O(log n) insert, erase, contains.
template<typename K,
         typename Cmp = decltype([](const K& a, const K& b){ return a < b; })>
class TreeSet {
    struct Node {
        K     key;
        Node* left   = nullptr;
        Node* right  = nullptr;
        Node* parent = nullptr;
        i32   height = 1;

        explicit Node(const K& k)  : key(k) {}
        explicit Node(K&& k)       : key(traits::move(k)) {}
    };

    using NodeAlloc = Allocator<Node>;

    // ── AVL helpers ───────────────────────────────────────────────────────────
    static i32 ht(const Node* n) noexcept { return n ? n->height : 0; }
    static i32 bf(const Node* n) noexcept { return ht(n->left) - ht(n->right); }
    static void upd(Node* n)     noexcept { i32 l=ht(n->left),r=ht(n->right); n->height=1+(l>r?l:r); }

    static Node* leftmost(Node* n)  noexcept { while (n && n->left)  n = n->left;  return n; }
    static Node* rightmost(Node* n) noexcept { while (n && n->right) n = n->right; return n; }

    static Node* succ(Node* n) noexcept {
        if (n->right) return leftmost(n->right);
        Node* p = n->parent;
        while (p && n == p->right) { n = p; p = p->parent; }
        return p;
    }
    static Node* pred(Node* n) noexcept {
        if (n->left) return rightmost(n->left);
        Node* p = n->parent;
        while (p && n == p->left) { n = p; p = p->parent; }
        return p;
    }

    Node* rot_right(Node* y) noexcept {
        Node* x = y->left; Node* T2 = x->right;
        x->right = y; y->left = T2;
        x->parent = y->parent; y->parent = x;
        if (T2) T2->parent = y;
        if (x->parent) { if (x->parent->left==y) x->parent->left=x; else x->parent->right=x; }
        else root_ = x;
        upd(y); upd(x); return x;
    }
    Node* rot_left(Node* x) noexcept {
        Node* y = x->right; Node* T2 = y->left;
        y->left = x; x->right = T2;
        y->parent = x->parent; x->parent = y;
        if (T2) T2->parent = x;
        if (y->parent) { if (y->parent->left==x) y->parent->left=y; else y->parent->right=y; }
        else root_ = y;
        upd(x); upd(y); return y;
    }
    Node* rebal(Node* n) noexcept {
        upd(n); i32 b = bf(n);
        if (b >  1) { if (bf(n->left)  < 0) rot_left(n->left);  return rot_right(n); }
        if (b < -1) { if (bf(n->right) > 0) rot_right(n->right); return rot_left(n); }
        return n;
    }
    void rebal_up(Node* n) noexcept { while (n) { Node* p = n->parent; rebal(n); n = p; } }

public:
    // ── Iterator (in-order) ───────────────────────────────────────────────────
    struct Iterator : BidirectionalIterator<Iterator, const K> {
        Node* n;
        explicit Iterator(Node* n) noexcept : n(n) {}
        Iterator& increment() noexcept { n = succ(n); return *this; }
        Iterator& decrement() noexcept { n = pred(n); return *this; }
        const K& deref() const noexcept { return n->key; }
        bool equal(Iterator o) const noexcept { return n == o.n; }
    };

    using iterator       = Iterator;
    using const_iterator = Iterator;

    // ── Construction ─────────────────────────────────────────────────────────
    TreeSet() : root_(nullptr), size_(0) {}
    TreeSet(const TreeSet& o) : root_(nullptr), size_(0) { for (const auto& k : o) insert(k); }
    TreeSet(TreeSet&& o) noexcept : root_(o.root_), size_(o.size_) { o.root_=nullptr; o.size_=0; }
    ~TreeSet() { destroy(root_); }

    TreeSet& operator=(const TreeSet& o) { if (this!=&o){ TreeSet t(o); *this=traits::move(t); } return *this; }
    TreeSet& operator=(TreeSet&& o) noexcept {
        if (this!=&o){ destroy(root_); root_=o.root_; size_=o.size_; o.root_=nullptr; o.size_=0; }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    bool insert(const K& k) { return ins_impl(k); }
    bool insert(K&& k)      { return ins_impl(traits::move(k)); }

    bool erase(const K& k) noexcept {
        Node* n = find_node(k);
        if (!n) return false;
        erase_node(n);
        return true;
    }

    void clear() noexcept { destroy(root_); root_ = nullptr; size_ = 0; }

    // ── Lookup ────────────────────────────────────────────────────────────────
    [[nodiscard]] bool contains(const K& k) const noexcept { return find_node(k) != nullptr; }

    [[nodiscard]] Optional<const K*> min_key() const noexcept {
        if (!root_) return none_of<const K*>();
        return some<const K*>(&leftmost(root_)->key);
    }
    [[nodiscard]] Optional<const K*> max_key() const noexcept {
        if (!root_) return none_of<const K*>();
        return some<const K*>(&rightmost(root_)->key);
    }

    // ── Iterators ─────────────────────────────────────────────────────────────
    Iterator begin() const noexcept { return Iterator(leftmost(root_)); }
    Iterator end()   const noexcept { return Iterator(nullptr); }

private:
    Node* root_;
    usize size_;
    Cmp   cmp_{};

    Node* find_node(const K& k) const noexcept {
        Node* c = root_;
        while (c) {
            if (cmp_(k, c->key))      c = c->left;
            else if (cmp_(c->key, k)) c = c->right;
            else return c;
        }
        return nullptr;
    }

    template<typename KK>
    bool ins_impl(KK&& k) {
        Node* parent = nullptr; Node** link = &root_;
        while (*link) {
            parent = *link;
            if (cmp_(k, parent->key))      link = &parent->left;
            else if (cmp_(parent->key, k)) link = &parent->right;
            else return false;
        }
        Node* n = NodeAlloc::allocate(1);
        new (n) Node(traits::forward<KK>(k));
        n->parent = parent; *link = n; ++size_;
        rebal_up(parent);
        return true;
    }

    void erase_node(Node* n) noexcept {
        if (n->left && n->right) {
            Node* s = leftmost(n->right);
            traits::swap(const_cast<K&>(n->key), const_cast<K&>(s->key));
            n = s;
        }
        Node* child = n->left ? n->left : n->right;
        Node* parent = n->parent;
        if (child) child->parent = parent;
        if (parent) { if (parent->left==n) parent->left=child; else parent->right=child; }
        else root_ = child;
        n->~Node(); NodeAlloc::deallocate(n); --size_;
        rebal_up(parent);
    }

    void destroy(Node* n) noexcept {
        if (!n) return;
        destroy(n->left); destroy(n->right);
        n->~Node(); NodeAlloc::deallocate(n);
    }
};

} // namespace dsc
