#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/allocator.hpp"
#include "../core/optional.hpp"
#include "../core/iterator.hpp"

namespace dsc {

// ── LinkedList<T> ─────────────────────────────────────────────────────────────
// Doubly-linked list with two sentinel (dummy) nodes.
//
// Sentinel design: head_ <-> [data nodes] <-> tail_
//   • Eliminates every null check in insert/erase hot paths.
//   • O(1) push/pop on both ends without special-casing.
//   • Cache-locality is intentionally sacrificed here (nodes heap-allocated)
//     in exchange for O(1) arbitrary insert/erase with stable iterators.
template<typename T>
class LinkedList {
    struct Node {
        T     data;
        Node* prev;
        Node* next;

        // Sentinel constructor (no data constructed)
        Node() noexcept : prev(nullptr), next(nullptr) {}

        template<typename... Args>
        Node(Node* p, Node* n, Args&&... args)
            : data(traits::forward<Args>(args)...), prev(p), next(n) {}
    };

    using NodeAlloc = Allocator<Node>;

public:
    // ── Iterator ──────────────────────────────────────────────────────────────
    struct Iterator : BidirectionalIterator<Iterator, T> {
        Node* n;
        explicit Iterator(Node* n) noexcept : n(n) {}
        Iterator& increment() noexcept { n = n->next; return *this; }
        Iterator& decrement() noexcept { n = n->prev; return *this; }
        T& deref() const noexcept { return n->data; }
        bool equal(Iterator o) const noexcept { return n == o.n; }
    };

    struct ConstIterator : BidirectionalIterator<ConstIterator, const T> {
        const Node* n;
        explicit ConstIterator(const Node* n) noexcept : n(n) {}
        ConstIterator& increment() noexcept { n = n->next; return *this; }
        ConstIterator& decrement() noexcept { n = n->prev; return *this; }
        const T& deref() const noexcept { return n->data; }
        bool equal(ConstIterator o) const noexcept { return n == o.n; }
    };

    using value_type = T;
    using iterator   = Iterator;
    using const_iterator = ConstIterator;
    using size_type  = usize;

    // ── Construction ─────────────────────────────────────────────────────────
    LinkedList() : size_(0) {
        head_ = NodeAlloc::allocate(1);
        tail_ = NodeAlloc::allocate(1);
        new (head_) Node();
        new (tail_) Node();
        head_->next = tail_;
        tail_->prev = head_;
    }

    LinkedList(const LinkedList& o) : LinkedList() {
        for (const auto& v : o) push_back(v);
    }

    LinkedList(LinkedList&& o) noexcept
        : head_(o.head_), tail_(o.tail_), size_(o.size_)
    {
        o.head_ = nullptr; o.tail_ = nullptr; o.size_ = 0;
    }

    ~LinkedList() {
        clear();
        if (head_) { head_->~Node(); NodeAlloc::deallocate(head_); }
        if (tail_) { tail_->~Node(); NodeAlloc::deallocate(tail_); }
    }

    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) { clear(); for (const auto& v : o) push_back(v); }
        return *this;
    }

    LinkedList& operator=(LinkedList&& o) noexcept {
        if (this != &o) {
            clear();
            head_->~Node(); NodeAlloc::deallocate(head_);
            tail_->~Node(); NodeAlloc::deallocate(tail_);
            head_ = o.head_; tail_ = o.tail_; size_ = o.size_;
            o.head_ = nullptr; o.tail_ = nullptr; o.size_ = 0;
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────────────
    [[nodiscard]] usize size()  const noexcept { return size_; }
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Element access ────────────────────────────────────────────────────────
    T&       front()       noexcept { return head_->next->data; }
    const T& front() const noexcept { return head_->next->data; }
    T&       back()        noexcept { return tail_->prev->data; }
    const T& back()  const noexcept { return tail_->prev->data; }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void push_back(const T& v)  { link_before(tail_, v); }
    void push_back(T&& v)       { link_before(tail_, traits::move(v)); }
    void push_front(const T& v) { link_before(head_->next, v); }
    void push_front(T&& v)      { link_before(head_->next, traits::move(v)); }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        return link_before_emplace(tail_, traits::forward<Args>(args)...)->data;
    }

    template<typename... Args>
    T& emplace_front(Args&&... args) {
        return link_before_emplace(head_->next, traits::forward<Args>(args)...)->data;
    }

    void pop_back()  noexcept { if (!empty()) unlink(tail_->prev); }
    void pop_front() noexcept { if (!empty()) unlink(head_->next); }

    Iterator insert(Iterator pos, const T& v) { return Iterator(link_before(pos.n, v)); }
    Iterator insert(Iterator pos, T&& v)      { return Iterator(link_before(pos.n, traits::move(v))); }

    Iterator erase(Iterator pos) noexcept {
        Node* next = pos.n->next;
        unlink(pos.n);
        return Iterator(next);
    }

    void clear() noexcept {
        if (!head_) return;
        Node* cur = head_->next;
        while (cur != tail_) {
            Node* nxt = cur->next;
            cur->~Node();
            NodeAlloc::deallocate(cur);
            cur = nxt;
        }
        head_->next = tail_;
        tail_->prev = head_;
        size_ = 0;
    }

    // ── Search ────────────────────────────────────────────────────────────────
    template<typename U>
    [[nodiscard]] Iterator find(const U& v) noexcept {
        for (auto it = begin(); it != end(); ++it) if (*it == v) return it;
        return end();
    }

    template<typename U>
    [[nodiscard]] bool contains(const U& v) const noexcept {
        for (const auto& x : *this) if (x == v) return true;
        return false;
    }

    // ── Iterators ─────────────────────────────────────────────────────────────
    Iterator      begin()        noexcept { return Iterator(head_->next); }
    Iterator      end()          noexcept { return Iterator(tail_); }
    ConstIterator begin()  const noexcept { return ConstIterator(head_->next); }
    ConstIterator end()    const noexcept { return ConstIterator(tail_); }
    ConstIterator cbegin() const noexcept { return begin(); }
    ConstIterator cend()   const noexcept { return end(); }

private:
    Node* head_;
    Node* tail_;
    usize size_;

    template<typename U>
    Node* link_before(Node* pos, U&& v) {
        Node* n = NodeAlloc::allocate(1);
        new (n) Node(pos->prev, pos, traits::forward<U>(v));
        pos->prev->next = n;
        pos->prev = n;
        ++size_;
        return n;
    }

    template<typename... Args>
    Node* link_before_emplace(Node* pos, Args&&... args) {
        Node* n = NodeAlloc::allocate(1);
        new (n) Node(pos->prev, pos, traits::forward<Args>(args)...);
        pos->prev->next = n;
        pos->prev = n;
        ++size_;
        return n;
    }

    void unlink(Node* n) noexcept {
        n->prev->next = n->next;
        n->next->prev = n->prev;
        n->~Node();
        NodeAlloc::deallocate(n);
        --size_;
    }
};

} // namespace dsc
