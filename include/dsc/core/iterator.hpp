#pragma once

#include "primitives.hpp"
#include "traits.hpp"

namespace dsc {

// ── Iterator category tags ────────────────────────────────────────────────────

/// @brief Tag type denoting a forward iterator (single-pass, increment-only).
struct forward_iterator_tag {};

/// @brief Tag type denoting a bidirectional iterator (supports both increment and decrement).
/// @note  Inherits from `forward_iterator_tag` for tag-dispatch hierarchies.
struct bidirectional_iterator_tag : forward_iterator_tag {};

/// @brief Tag type denoting a random-access iterator (supports arbitrary ± offsets).
/// @note  Inherits from `bidirectional_iterator_tag` for tag-dispatch hierarchies.
struct random_access_iterator_tag : bidirectional_iterator_tag {};

// ── CRTP RandomAccessIterator ─────────────────────────────────────────────────
// Derive from this and implement four primitives:
//   Derived& advance(isize n)          – move by n steps
//   T&       deref()  const            – dereference
//   isize    distance_to(Derived) const – signed distance to rhs
//   bool     equal(Derived)      const – equality
//
// All other operators are synthesized automatically — zero overhead at -O2.

/// @brief CRTP base class that synthesises a complete random-access iterator interface.
///
/// Derive from this template and implement exactly four primitives in `Derived`:
///   - `Derived& advance(isize n)` — advance the iterator by `n` positions (negative = backward).
///   - `T& deref() const`          — return a reference to the element at the current position.
///   - `isize distance_to(Derived) const` — signed number of steps from `*this` to the argument.
///   - `bool equal(Derived) const`  — return `true` when two iterators point to the same position.
///
/// All remaining iterator operators (`++`, `--`, `+`, `-`, `[]`, comparison) are synthesised
/// automatically with zero overhead at `-O2`.
///
/// @tparam Derived  The concrete iterator type that inherits from this base (CRTP).
/// @tparam T        The element type that the iterator points to.
template<typename Derived, typename T>
class RandomAccessIterator {
    /// @brief CRTP downcast to mutable derived reference.
    /// @return Mutable reference to the derived iterator object.
    Derived&       self()       noexcept { return static_cast<Derived&>(*this); }
    /// @brief CRTP downcast to const derived reference.
    /// @return Const reference to the derived iterator object.
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

public:
    /// @brief Iterator category tag — `random_access_iterator_tag`.
    using iterator_category = random_access_iterator_tag;
    /// @brief The type of the elements pointed to.
    using value_type        = T;
    /// @brief Signed difference type used for iterator arithmetic.
    using difference_type   = isize;
    /// @brief Pointer to the element type.
    using pointer           = T*;
    /// @brief Reference to the element type.
    using reference         = T&;

    /// @brief Pre-increment: advance the iterator by one step.
    /// @return Reference to `*this` after advancing.
    /// @complexity O(1)
    Derived& operator++()    noexcept { return self().advance(1); }

    /// @brief Post-increment: advance the iterator by one step, returning the old position.
    /// @return Copy of the iterator before advancing.
    /// @complexity O(1)
    Derived  operator++(int) noexcept { Derived t = self(); self().advance(1);  return t; }

    /// @brief Pre-decrement: retreat the iterator by one step.
    /// @return Reference to `*this` after retreating.
    /// @complexity O(1)
    Derived& operator--()    noexcept { return self().advance(-1); }

    /// @brief Post-decrement: retreat the iterator by one step, returning the old position.
    /// @return Copy of the iterator before retreating.
    /// @complexity O(1)
    Derived  operator--(int) noexcept { Derived t = self(); self().advance(-1); return t; }

    /// @brief Advance the iterator by `n` positions in-place.
    /// @param n  Number of positions to advance (may be negative).
    /// @return   Reference to `*this`.
    /// @complexity O(1)
    Derived& operator+=(isize n) noexcept { return self().advance(n);  }

    /// @brief Retreat the iterator by `n` positions in-place.
    /// @param n  Number of positions to retreat (may be negative).
    /// @return   Reference to `*this`.
    /// @complexity O(1)
    Derived& operator-=(isize n) noexcept { return self().advance(-n); }

    /// @brief Return a new iterator advanced by `n` positions.
    /// @param n  Offset to apply.
    /// @return   New iterator at position `*this + n`.
    /// @complexity O(1)
    [[nodiscard]] Derived operator+(isize n) const noexcept { Derived t=self(); t.advance(n);  return t; }

    /// @brief Return a new iterator retreated by `n` positions.
    /// @param n  Offset to subtract.
    /// @return   New iterator at position `*this - n`.
    /// @complexity O(1)
    [[nodiscard]] Derived operator-(isize n) const noexcept { Derived t=self(); t.advance(-n); return t; }

    /// @brief Compute the signed distance between two iterators.
    /// @param o  The other iterator (right-hand side).
    /// @return   Number of steps from `o` to `*this` (positive when `*this` is after `o`).
    /// @complexity O(1)
    [[nodiscard]] isize operator-(const Derived& o) const noexcept {
        return o.distance_to(self());
    }

    /// @brief Dereference — returns the element at the current iterator position.
    /// @return Reference to the element.
    /// @pre   The iterator must be dereferenceable (not past-the-end).
    /// @complexity O(1)
    [[nodiscard]] T& operator*()  const noexcept { return const_cast<Derived&>(self()).deref(); }

    /// @brief Arrow dereference — returns a pointer to the current element.
    /// @return Pointer to the element.
    /// @pre   The iterator must be dereferenceable.
    /// @complexity O(1)
    [[nodiscard]] T* operator->() const noexcept { return &const_cast<Derived&>(self()).deref(); }

    /// @brief Subscript — returns the element `n` positions from the current position.
    /// @param n  Offset from the current position.
    /// @return   Reference to the element at `*this + n`.
    /// @complexity O(1)
    [[nodiscard]] T& operator[](isize n) const noexcept { return *(self() + n); }

    /// @brief Equality comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if both iterators point to the same position.
    [[nodiscard]] bool operator==(const Derived& o) const noexcept { return  self().equal(o); }

    /// @brief Inequality comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if the iterators point to different positions.
    [[nodiscard]] bool operator!=(const Derived& o) const noexcept { return !self().equal(o); }

    /// @brief Less-than comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if `*this` is before `o`.
    [[nodiscard]] bool operator< (const Derived& o) const noexcept { return self().distance_to(o) > 0; }

    /// @brief Greater-than comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if `*this` is after `o`.
    [[nodiscard]] bool operator> (const Derived& o) const noexcept { return o < self(); }

    /// @brief Less-than-or-equal comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if `*this` is at or before `o`.
    [[nodiscard]] bool operator<=(const Derived& o) const noexcept { return !(self() > o); }

    /// @brief Greater-than-or-equal comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if `*this` is at or after `o`.
    [[nodiscard]] bool operator>=(const Derived& o) const noexcept { return !(self() < o); }

    /// @brief Non-member `n + it` — symmetric addition with an integer on the left.
    /// @param n   Scalar offset.
    /// @param it  The iterator.
    /// @return    New iterator at `it + n`.
    friend Derived operator+(isize n, const Derived& it) noexcept { return it + n; }
};

// ── CRTP BidirectionalIterator ────────────────────────────────────────────────
// Implement: increment(), decrement(), deref(), equal().

/// @brief CRTP base class that synthesises a complete bidirectional iterator interface.
///
/// Derive from this template and implement exactly four primitives in `Derived`:
///   - `Derived& increment()` — advance to the next element.
///   - `Derived& decrement()` — retreat to the previous element.
///   - `T& deref() const`     — return a reference to the current element.
///   - `bool equal(Derived) const` — return `true` when two iterators are at the same position.
///
/// Increment, decrement, dereference, and equality operators are synthesised automatically.
///
/// @tparam Derived  The concrete iterator type that inherits from this base (CRTP).
/// @tparam T        The element type that the iterator points to.
template<typename Derived, typename T>
class BidirectionalIterator {
    /// @brief CRTP downcast to mutable derived reference.
    /// @return Mutable reference to the derived iterator object.
    Derived&       self()       noexcept { return static_cast<Derived&>(*this); }
    /// @brief CRTP downcast to const derived reference.
    /// @return Const reference to the derived iterator object.
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

public:
    /// @brief Iterator category tag — `bidirectional_iterator_tag`.
    using iterator_category = bidirectional_iterator_tag;
    /// @brief The type of the elements pointed to.
    using value_type        = T;
    /// @brief Signed difference type.
    using difference_type   = isize;
    /// @brief Pointer to the element type.
    using pointer           = T*;
    /// @brief Reference to the element type.
    using reference         = T&;

    /// @brief Pre-increment: advance to the next element.
    /// @return Reference to `*this` after the advance.
    /// @complexity O(1)
    Derived& operator++()    noexcept { return self().increment(); }

    /// @brief Post-increment: advance to the next element, returning the old position.
    /// @return Copy of the iterator before advancing.
    /// @complexity O(1)
    Derived  operator++(int) noexcept { Derived t = self(); self().increment(); return t; }

    /// @brief Pre-decrement: retreat to the previous element.
    /// @return Reference to `*this` after retreating.
    /// @complexity O(1)
    Derived& operator--()    noexcept { return self().decrement(); }

    /// @brief Post-decrement: retreat to the previous element, returning the old position.
    /// @return Copy of the iterator before retreating.
    /// @complexity O(1)
    Derived  operator--(int) noexcept { Derived t = self(); self().decrement(); return t; }

    /// @brief Dereference — returns the element at the current iterator position.
    /// @return Reference to the element.
    /// @pre   The iterator must be dereferenceable.
    /// @complexity O(1)
    [[nodiscard]] T& operator*()  const noexcept { return const_cast<Derived&>(self()).deref(); }

    /// @brief Arrow dereference — returns a pointer to the current element.
    /// @return Pointer to the element.
    /// @pre   The iterator must be dereferenceable.
    /// @complexity O(1)
    [[nodiscard]] T* operator->() const noexcept { return &const_cast<Derived&>(self()).deref(); }

    /// @brief Equality comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if both iterators point to the same position.
    [[nodiscard]] bool operator==(const Derived& o) const noexcept { return  self().equal(o); }

    /// @brief Inequality comparison.
    /// @param o  Iterator to compare with.
    /// @return   `true` if the iterators point to different positions.
    [[nodiscard]] bool operator!=(const Derived& o) const noexcept { return !self().equal(o); }
};

} // namespace dsc
