#pragma once

#include "array.hpp"

namespace dsc {

// ── Stack<T> ──────────────────────────────────────────────────────────────────
// LIFO container backed by Array<T> — benefits from all Array optimizations.
// O(1) push, O(1) pop, O(1) top.

/// @brief Last-in, first-out (LIFO) container adapter backed by `Array<T>`.
///
/// All operations inherit the cache-friendly, contiguous-memory properties of
/// `Array<T>`. Growth uses the same 1.5× policy, and trivially copyable types
/// receive memcpy fast-paths on reallocation.
///
/// @tparam T Element type. Must satisfy the same requirements as `Array<T>`.
template<typename T>
class Stack {
public:
    /// @brief Default constructor. Creates an empty stack.
    /// @complexity O(1).
    Stack()                        = default;

    /// @brief Copy constructor. Deep-copies all elements from @p other.
    /// @complexity O(n).
    Stack(const Stack&)            = default;

    /// @brief Move constructor. Transfers ownership from @p other.
    /// @complexity O(1).
    Stack(Stack&&)                 = default;

    /// @brief Copy-assignment operator.
    /// @return Reference to `*this`.
    /// @complexity O(n).
    Stack& operator=(const Stack&) = default;

    /// @brief Move-assignment operator.
    /// @return Reference to `*this`.
    /// @complexity O(1).
    Stack& operator=(Stack&&)      = default;

    // Push a copy

    /// @brief Pushes a copy of @p v onto the top of the stack.
    /// @param v Value to copy onto the stack.
    /// @note May trigger a 1.5× reallocation of the underlying buffer.
    /// @complexity O(1) amortised.
    void push(const T& v) { buf_.push_back(v); }

    // Push via move

    /// @brief Pushes @p v onto the top of the stack by move.
    /// @param v Value to move onto the stack.
    /// @note May trigger a 1.5× reallocation of the underlying buffer.
    /// @complexity O(1) amortised.
    void push(T&& v)      { buf_.push_back(traits::move(v)); }

    // Construct in-place at top

    /// @brief Constructs a new element in-place at the top of the stack.
    /// @tparam Args Constructor argument types forwarded to `T`'s constructor.
    /// @param  args Arguments forwarded to the constructor of `T`.
    /// @return Mutable reference to the newly constructed top element.
    /// @note May trigger a 1.5× reallocation of the underlying buffer.
    /// @complexity O(1) amortised.
    template<typename... Args>
    T& emplace(Args&&... args) { return buf_.emplace_back(traits::forward<Args>(args)...); }

    // Remove top element

    /// @brief Removes the top element from the stack.
    /// @note Has no effect if the stack is empty.
    /// @complexity O(1).
    void pop() noexcept { buf_.pop_back(); }

    // Access top element (undefined behaviour on empty stack)

    /// @brief Returns a mutable reference to the top element.
    /// @return Reference to the element most recently pushed.
    /// @pre `!empty()`. Behaviour is undefined on an empty stack.
    /// @complexity O(1).
    [[nodiscard]] T&       top()       noexcept { return buf_.back(); }

    /// @brief Returns a const reference to the top element.
    /// @return Const reference to the element most recently pushed.
    /// @pre `!empty()`. Behaviour is undefined on an empty stack.
    /// @complexity O(1).
    [[nodiscard]] const T& top() const noexcept { return buf_.back(); }

    // Remove and return top element

    /// @brief Removes and returns the top element by value.
    ///
    /// The element is move-constructed into the return value before the
    /// backing array removes it, so no extra copy occurs for move-only types.
    /// @return The element that was on top of the stack.
    /// @pre `!empty()`. Behaviour is undefined on an empty stack.
    /// @complexity O(1).
    [[nodiscard]] T pop_value() {
        T v = traits::move(buf_.back());
        buf_.pop_back();
        return v;
    }

    /// @brief Returns the number of elements currently in the stack.
    /// @return Current element count.
    /// @complexity O(1).
    [[nodiscard]] usize size()  const noexcept { return buf_.size(); }

    /// @brief Returns `true` if the stack contains no elements.
    /// @return `true` when `size() == 0`.
    /// @complexity O(1).
    [[nodiscard]] bool  empty() const noexcept { return buf_.empty(); }

    /// @brief Destroys all elements, leaving `size() == 0`.
    /// @note The underlying buffer allocation is retained.
    /// @complexity O(n) for non-trivially destructible `T`; O(1) otherwise.
    void clear() noexcept { buf_.clear(); }

private:
    /// @brief Underlying contiguous storage for stack elements.
    Array<T> buf_;
};

} // namespace dsc
