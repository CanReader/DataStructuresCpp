#pragma once

#include "array.hpp"

namespace dsc {

// ── Stack<T> ──────────────────────────────────────────────────────────────────
// LIFO container backed by Array<T> — benefits from all Array optimizations.
// O(1) push, O(1) pop, O(1) top.
template<typename T>
class Stack {
public:
    Stack()                        = default;
    Stack(const Stack&)            = default;
    Stack(Stack&&)                 = default;
    Stack& operator=(const Stack&) = default;
    Stack& operator=(Stack&&)      = default;

    // Push a copy
    void push(const T& v) { buf_.push_back(v); }
    // Push via move
    void push(T&& v)      { buf_.push_back(traits::move(v)); }

    // Construct in-place at top
    template<typename... Args>
    T& emplace(Args&&... args) { return buf_.emplace_back(traits::forward<Args>(args)...); }

    // Remove top element
    void pop() noexcept { buf_.pop_back(); }

    // Access top element (undefined behaviour on empty stack)
    [[nodiscard]] T&       top()       noexcept { return buf_.back(); }
    [[nodiscard]] const T& top() const noexcept { return buf_.back(); }

    // Remove and return top element
    [[nodiscard]] T pop_value() {
        T v = traits::move(buf_.back());
        buf_.pop_back();
        return v;
    }

    [[nodiscard]] usize size()  const noexcept { return buf_.size(); }
    [[nodiscard]] bool  empty() const noexcept { return buf_.empty(); }
    void clear() noexcept { buf_.clear(); }

private:
    Array<T> buf_;
};

} // namespace dsc
