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

/// @brief Doubly-linked list with sentinel (dummy) head and tail nodes.
///
/// Uses a sentinel design — `head_` and `tail_` are never-null dummy nodes that
/// bracket the actual data nodes (`head_ <-> [data ...] <-> tail_`). This
/// eliminates null-pointer checks in every insertion and erasure hot-path and
/// enables O(1) push/pop at both ends without any special-casing.
///
/// Iterators remain valid across insertions and are invalidated only by the
/// erasure of the node they refer to.
///
/// @tparam T Element type. Must be constructible from the arguments passed to
///           `emplace_front` / `emplace_back`.
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

    /// @brief Mutable bidirectional iterator over `LinkedList<T>` nodes.
    ///
    /// Wraps a raw `Node*` and satisfies the `BidirectionalIterator` CRTP
    /// contract. `end()` points to the tail sentinel node; dereferencing it
    /// is undefined behaviour.
    struct Iterator : BidirectionalIterator<Iterator, T> {
        /// @brief Pointer to the current list node.
        Node* n;

        /// @brief Constructs an iterator wrapping @p n.
        /// @param n Pointer to the node this iterator should refer to.
        explicit Iterator(Node* n) noexcept : n(n) {}

        /// @brief Advances to the next node (pre-increment helper).
        /// @return Reference to `*this` after moving forward.
        Iterator& increment() noexcept { n = n->next; return *this; }

        /// @brief Retreats to the previous node (pre-decrement helper).
        /// @return Reference to `*this` after moving backward.
        Iterator& decrement() noexcept { n = n->prev; return *this; }

        /// @brief Dereferences the iterator to obtain the stored element.
        /// @return Mutable reference to the element in the current node.
        /// @pre The iterator must not equal `end()`.
        T& deref() const noexcept { return n->data; }

        /// @brief Tests whether two iterators refer to the same node.
        /// @param o Iterator to compare with.
        /// @return `true` if both iterators wrap the same node pointer.
        bool equal(Iterator o) const noexcept { return n == o.n; }
    };

    /// @brief Immutable bidirectional iterator over `LinkedList<T>` nodes.
    ///
    /// Identical to `Iterator` but yields `const T&` on dereference, allowing
    /// read-only traversal of `const LinkedList` instances.
    struct ConstIterator : BidirectionalIterator<ConstIterator, const T> {
        /// @brief Pointer to the current (read-only) list node.
        const Node* n;

        /// @brief Constructs a const iterator wrapping @p n.
        /// @param n Pointer to the node this iterator should refer to.
        explicit ConstIterator(const Node* n) noexcept : n(n) {}

        /// @brief Advances to the next node (pre-increment helper).
        /// @return Reference to `*this` after moving forward.
        ConstIterator& increment() noexcept { n = n->next; return *this; }

        /// @brief Retreats to the previous node (pre-decrement helper).
        /// @return Reference to `*this` after moving backward.
        ConstIterator& decrement() noexcept { n = n->prev; return *this; }

        /// @brief Dereferences the iterator to obtain the stored element.
        /// @return Const reference to the element in the current node.
        /// @pre The iterator must not equal `cend()`.
        const T& deref() const noexcept { return n->data; }

        /// @brief Tests whether two const iterators refer to the same node.
        /// @param o Const iterator to compare with.
        /// @return `true` if both iterators wrap the same node pointer.
        bool equal(ConstIterator o) const noexcept { return n == o.n; }
    };

    /// @brief Type of stored elements.
    using value_type = T;
    /// @brief Mutable iterator type (alias for `Iterator`).
    using iterator   = Iterator;
    /// @brief Immutable iterator type (alias for `ConstIterator`).
    using const_iterator = ConstIterator;
    /// @brief Unsigned integral type used for the element count.
    using size_type  = usize;

    // ── Construction ─────────────────────────────────────────────────────────

    /// @brief Default constructor. Creates an empty list with two sentinel nodes.
    /// @note Allocates head and tail sentinels on the heap; no element storage
    ///       is allocated until elements are inserted.
    /// @complexity O(1).
    LinkedList() : size_(0) {
        head_ = NodeAlloc::allocate(1);
        tail_ = NodeAlloc::allocate(1);
        new (head_) Node();
        new (tail_) Node();
        head_->next = tail_;
        tail_->prev = head_;
    }

    /// @brief Copy constructor. Deep-copies all elements from @p o.
    /// @param o Source list to copy from.
    /// @complexity O(n).
    LinkedList(const LinkedList& o) : LinkedList() {
        for (const auto& v : o) push_back(v);
    }

    /// @brief Move constructor. Transfers ownership of all nodes from @p o.
    /// @param o Source list to move from. Left in an empty, invalid state
    ///          (sentinels set to `nullptr`) after the operation.
    /// @complexity O(1).
    LinkedList(LinkedList&& o) noexcept
        : head_(o.head_), tail_(o.tail_), size_(o.size_)
    {
        o.head_ = nullptr; o.tail_ = nullptr; o.size_ = 0;
    }

    /// @brief Destructor. Destroys all data nodes and the two sentinel nodes.
    /// @complexity O(n).
    ~LinkedList() {
        clear();
        if (head_) { head_->~Node(); NodeAlloc::deallocate(head_); }
        if (tail_) { tail_->~Node(); NodeAlloc::deallocate(tail_); }
    }

    /// @brief Copy-assignment operator. Replaces contents with a copy of @p o.
    /// @param o Source list to copy from.
    /// @return Reference to `*this`.
    /// @complexity O(n).
    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) { clear(); for (const auto& v : o) push_back(v); }
        return *this;
    }

    /// @brief Move-assignment operator. Transfers ownership from @p o.
    /// @param o Source list to move from. Left in an empty, invalid state
    ///          after the operation.
    /// @return Reference to `*this`.
    /// @complexity O(n) to destroy current elements; O(1) for the transfer.
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

    /// @brief Returns the number of elements currently in the list.
    /// @return Current element count.
    /// @complexity O(1).
    [[nodiscard]] usize size()  const noexcept { return size_; }

    /// @brief Returns `true` if the list contains no elements.
    /// @return `true` when `size() == 0`.
    /// @complexity O(1).
    [[nodiscard]] bool  empty() const noexcept { return size_ == 0; }

    // ── Element access ────────────────────────────────────────────────────────

    /// @brief Returns a mutable reference to the first element.
    /// @return Reference to the data stored in the node after the head sentinel.
    /// @pre `!empty()`.
    /// @complexity O(1).
    T&       front()       noexcept { return head_->next->data; }

    /// @brief Returns a const reference to the first element.
    /// @return Const reference to the data stored in the node after the head sentinel.
    /// @pre `!empty()`.
    /// @complexity O(1).
    const T& front() const noexcept { return head_->next->data; }

    /// @brief Returns a mutable reference to the last element.
    /// @return Reference to the data stored in the node before the tail sentinel.
    /// @pre `!empty()`.
    /// @complexity O(1).
    T&       back()        noexcept { return tail_->prev->data; }

    /// @brief Returns a const reference to the last element.
    /// @return Const reference to the data stored in the node before the tail sentinel.
    /// @pre `!empty()`.
    /// @complexity O(1).
    const T& back()  const noexcept { return tail_->prev->data; }

    // ── Modifiers ─────────────────────────────────────────────────────────────

    /// @brief Appends a copy of @p v to the end of the list.
    /// @param v Value to copy-append.
    /// @complexity O(1).
    void push_back(const T& v)  { link_before(tail_, v); }

    /// @brief Appends @p v to the end of the list by move.
    /// @param v Value to move-append.
    /// @complexity O(1).
    void push_back(T&& v)       { link_before(tail_, traits::move(v)); }

    /// @brief Prepends a copy of @p v to the front of the list.
    /// @param v Value to copy-prepend.
    /// @complexity O(1).
    void push_front(const T& v) { link_before(head_->next, v); }

    /// @brief Prepends @p v to the front of the list by move.
    /// @param v Value to move-prepend.
    /// @complexity O(1).
    void push_front(T&& v)      { link_before(head_->next, traits::move(v)); }

    /// @brief Constructs a new element in-place at the back of the list.
    /// @tparam Args Constructor argument types forwarded to `T`'s constructor.
    /// @param  args Arguments forwarded to the constructor of `T`.
    /// @return Mutable reference to the newly constructed element.
    /// @complexity O(1).
    template<typename... Args>
    T& emplace_back(Args&&... args) {
        return link_before_emplace(tail_, traits::forward<Args>(args)...)->data;
    }

    /// @brief Constructs a new element in-place at the front of the list.
    /// @tparam Args Constructor argument types forwarded to `T`'s constructor.
    /// @param  args Arguments forwarded to the constructor of `T`.
    /// @return Mutable reference to the newly constructed element.
    /// @complexity O(1).
    template<typename... Args>
    T& emplace_front(Args&&... args) {
        return link_before_emplace(head_->next, traits::forward<Args>(args)...)->data;
    }

    /// @brief Removes the last element.
    /// @note Has no effect if the list is empty.
    /// @complexity O(1).
    void pop_back()  noexcept { if (!empty()) unlink(tail_->prev); }

    /// @brief Removes the first element.
    /// @note Has no effect if the list is empty.
    /// @complexity O(1).
    void pop_front() noexcept { if (!empty()) unlink(head_->next); }

    /// @brief Inserts a copy of @p v before the element at @p pos.
    /// @param pos Iterator to the position before which to insert.
    /// @param v   Value to copy-insert.
    /// @return Iterator pointing to the newly inserted element.
    /// @complexity O(1).
    Iterator insert(Iterator pos, const T& v) { return Iterator(link_before(pos.n, v)); }

    /// @brief Inserts @p v before the element at @p pos by move.
    /// @param pos Iterator to the position before which to insert.
    /// @param v   Value to move-insert.
    /// @return Iterator pointing to the newly inserted element.
    /// @complexity O(1).
    Iterator insert(Iterator pos, T&& v)      { return Iterator(link_before(pos.n, traits::move(v))); }

    /// @brief Removes the element at @p pos.
    /// @param pos Iterator to the element to remove. Must be dereferenceable.
    /// @return Iterator to the element that followed @p pos (may equal `end()`).
    /// @pre @p pos must be a valid, dereferenceable iterator into this list.
    /// @complexity O(1).
    Iterator erase(Iterator pos) noexcept {
        Node* next = pos.n->next;
        unlink(pos.n);
        return Iterator(next);
    }

    /// @brief Destroys all elements, leaving `size() == 0`.
    /// @note The sentinel nodes are preserved.
    /// @complexity O(n).
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

    /// @brief Finds the first element equal to @p v.
    /// @tparam U Type comparable to `T` via `operator==`.
    /// @param  v Value to search for.
    /// @return Mutable iterator to the first matching element, or `end()` if
    ///         not found.
    /// @complexity O(n).
    template<typename U>
    [[nodiscard]] Iterator find(const U& v) noexcept {
        for (auto it = begin(); it != end(); ++it) if (*it == v) return it;
        return end();
    }

    /// @brief Tests whether any element equals @p v.
    /// @tparam U Type comparable to `T` via `operator==`.
    /// @param  v Value to search for.
    /// @return `true` if at least one element compares equal to @p v.
    /// @complexity O(n).
    template<typename U>
    [[nodiscard]] bool contains(const U& v) const noexcept {
        for (const auto& x : *this) if (x == v) return true;
        return false;
    }

    // ── Iterators ─────────────────────────────────────────────────────────────

    /// @brief Returns a mutable iterator to the first element.
    /// @return `Iterator` wrapping the first data node (or `end()` if empty).
    /// @complexity O(1).
    Iterator      begin()        noexcept { return Iterator(head_->next); }

    /// @brief Returns a mutable past-the-end iterator (points to tail sentinel).
    /// @return `Iterator` wrapping the tail sentinel node.
    /// @complexity O(1).
    Iterator      end()          noexcept { return Iterator(tail_); }

    /// @brief Returns a const iterator to the first element.
    /// @return `ConstIterator` wrapping the first data node (or `cend()` if empty).
    /// @complexity O(1).
    ConstIterator begin()  const noexcept { return ConstIterator(head_->next); }

    /// @brief Returns a const past-the-end iterator (points to tail sentinel).
    /// @return `ConstIterator` wrapping the tail sentinel node.
    /// @complexity O(1).
    ConstIterator end()    const noexcept { return ConstIterator(tail_); }

    /// @brief Returns a const iterator to the first element (explicit const).
    /// @return Same as `begin() const`.
    /// @complexity O(1).
    ConstIterator cbegin() const noexcept { return begin(); }

    /// @brief Returns a const past-the-end iterator (explicit const).
    /// @return Same as `end() const`.
    /// @complexity O(1).
    ConstIterator cend()   const noexcept { return end(); }

private:
    Node* head_;
    Node* tail_;
    usize size_;

    /// @brief Allocates a new node with value @p v and splices it in before @p pos.
    /// @tparam U   Deduced value type (lvalue or rvalue reference).
    /// @param pos  Node before which the new node is inserted.
    /// @param v    Forwarding reference to the value to store.
    /// @return Pointer to the newly allocated and linked node.
    /// @complexity O(1).
    template<typename U>
    Node* link_before(Node* pos, U&& v) {
        Node* n = NodeAlloc::allocate(1);
        new (n) Node(pos->prev, pos, traits::forward<U>(v));
        pos->prev->next = n;
        pos->prev = n;
        ++size_;
        return n;
    }

    /// @brief Allocates a new node constructed in-place with @p args and splices
    ///        it in before @p pos.
    /// @tparam Args Constructor argument types for `T`.
    /// @param pos   Node before which the new node is inserted.
    /// @param args  Arguments forwarded to `T`'s constructor.
    /// @return Pointer to the newly allocated and linked node.
    /// @complexity O(1).
    template<typename... Args>
    Node* link_before_emplace(Node* pos, Args&&... args) {
        Node* n = NodeAlloc::allocate(1);
        new (n) Node(pos->prev, pos, traits::forward<Args>(args)...);
        pos->prev->next = n;
        pos->prev = n;
        ++size_;
        return n;
    }

    /// @brief Splices @p n out of the list, destroys it, and frees its memory.
    /// @param n Pointer to the node to remove. Must be a valid data node
    ///          (not a sentinel).
    /// @complexity O(1).
    void unlink(Node* n) noexcept {
        n->prev->next = n->next;
        n->next->prev = n->prev;
        n->~Node();
        NodeAlloc::deallocate(n);
        --size_;
    }
};

} // namespace dsc
