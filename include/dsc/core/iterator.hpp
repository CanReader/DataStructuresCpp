#pragma once

#include "primitives.hpp"
#include "traits.hpp"

namespace dsc {

// ── Iterator category tags ────────────────────────────────────────────────────
struct forward_iterator_tag {};
struct bidirectional_iterator_tag : forward_iterator_tag {};
struct random_access_iterator_tag : bidirectional_iterator_tag {};

// ── CRTP RandomAccessIterator ─────────────────────────────────────────────────
// Derive from this and implement four primitives:
//   Derived& advance(isize n)          – move by n steps
//   T&       deref()  const            – dereference
//   isize    distance_to(Derived) const – signed distance to rhs
//   bool     equal(Derived)      const – equality
//
// All other operators are synthesized automatically — zero overhead at -O2.
template<typename Derived, typename T>
class RandomAccessIterator {
    Derived&       self()       noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

public:
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = isize;
    using pointer           = T*;
    using reference         = T&;

    Derived& operator++()    noexcept { return self().advance(1); }
    Derived  operator++(int) noexcept { Derived t = self(); self().advance(1);  return t; }
    Derived& operator--()    noexcept { return self().advance(-1); }
    Derived  operator--(int) noexcept { Derived t = self(); self().advance(-1); return t; }

    Derived& operator+=(isize n) noexcept { return self().advance(n);  }
    Derived& operator-=(isize n) noexcept { return self().advance(-n); }

    [[nodiscard]] Derived operator+(isize n) const noexcept { Derived t=self(); t.advance(n);  return t; }
    [[nodiscard]] Derived operator-(isize n) const noexcept { Derived t=self(); t.advance(-n); return t; }

    [[nodiscard]] isize operator-(const Derived& o) const noexcept {
        return o.distance_to(self());
    }

    [[nodiscard]] T& operator*()  const noexcept { return const_cast<Derived&>(self()).deref(); }
    [[nodiscard]] T* operator->() const noexcept { return &const_cast<Derived&>(self()).deref(); }
    [[nodiscard]] T& operator[](isize n) const noexcept { return *(self() + n); }

    [[nodiscard]] bool operator==(const Derived& o) const noexcept { return  self().equal(o); }
    [[nodiscard]] bool operator!=(const Derived& o) const noexcept { return !self().equal(o); }
    [[nodiscard]] bool operator< (const Derived& o) const noexcept { return self().distance_to(o) > 0; }
    [[nodiscard]] bool operator> (const Derived& o) const noexcept { return o < self(); }
    [[nodiscard]] bool operator<=(const Derived& o) const noexcept { return !(self() > o); }
    [[nodiscard]] bool operator>=(const Derived& o) const noexcept { return !(self() < o); }

    friend Derived operator+(isize n, const Derived& it) noexcept { return it + n; }
};

// ── CRTP BidirectionalIterator ────────────────────────────────────────────────
// Implement: increment(), decrement(), deref(), equal().
template<typename Derived, typename T>
class BidirectionalIterator {
    Derived&       self()       noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

public:
    using iterator_category = bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = isize;
    using pointer           = T*;
    using reference         = T&;

    Derived& operator++()    noexcept { return self().increment(); }
    Derived  operator++(int) noexcept { Derived t = self(); self().increment(); return t; }
    Derived& operator--()    noexcept { return self().decrement(); }
    Derived  operator--(int) noexcept { Derived t = self(); self().decrement(); return t; }

    [[nodiscard]] T& operator*()  const noexcept { return const_cast<Derived&>(self()).deref(); }
    [[nodiscard]] T* operator->() const noexcept { return &const_cast<Derived&>(self()).deref(); }

    [[nodiscard]] bool operator==(const Derived& o) const noexcept { return  self().equal(o); }
    [[nodiscard]] bool operator!=(const Derived& o) const noexcept { return !self().equal(o); }
};

} // namespace dsc
