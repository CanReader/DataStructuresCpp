#pragma once

#include "primitives.hpp"
#include "traits.hpp"

namespace dsc {

// ── Optional<T> ───────────────────────────────────────────────────────────────
// Lightweight nullable value container — replaces std::optional.
// Uses aligned storage so T is never default-constructed unless explicitly set.

/// @brief Lightweight nullable value container (replacement for `std::optional`).
/// @tparam T  The contained value type.
/// @note  Uses properly aligned stack storage (`alignas(T) unsigned char[sizeof(T)]`)
///        so `T` is never default-constructed unless an explicit value is assigned.
///        Trivially destructible types skip the destructor call entirely.
template<typename T>
class Optional {
    alignas(T) unsigned char storage_[sizeof(T)];
    bool has_;

    /// @brief Returns a typed pointer into the internal aligned storage.
    /// @return Mutable pointer to the contained object (may not be constructed).
    T*       ptr()       noexcept { return reinterpret_cast<T*>(storage_); }
    /// @brief Returns a const typed pointer into the internal aligned storage.
    /// @return Const pointer to the contained object (may not be constructed).
    const T* ptr() const noexcept { return reinterpret_cast<const T*>(storage_); }

public:
    /// @brief Constructs an empty (valueless) `Optional`.
    /// @post  `has_value()` returns `false`.
    constexpr Optional() noexcept : storage_{}, has_(false) {}

    /// @brief Constructs an `Optional` containing a copy of `v`.
    /// @param v  Value to copy-construct into the storage.
    /// @post  `has_value()` returns `true`.
    Optional(const T& v) noexcept(traits::is_trivially_constructible_v<T, const T&>)
        : has_(true) { new (storage_) T(v); }

    /// @brief Constructs an `Optional` by moving `v` into the storage.
    /// @param v  Value to move-construct into the storage.
    /// @post  `has_value()` returns `true`.
    Optional(T&& v) noexcept(traits::is_trivially_constructible_v<T, T&&>)
        : has_(true) { new (storage_) T(traits::move(v)); }

    /// @brief Copy constructor — copies the contained value if present.
    /// @param o  Source `Optional` to copy from.
    Optional(const Optional& o) : has_(o.has_) {
        if (has_) new (storage_) T(*o.ptr());
    }

    /// @brief Move constructor — transfers the contained value from `o`.
    /// @param o  Source `Optional` to move from; left empty after the call.
    Optional(Optional&& o) noexcept : has_(o.has_) {
        if (has_) {
            new (storage_) T(traits::move(*o.ptr()));
            o.reset();
        }
    }

    /// @brief Destructor — destroys the contained value if present.
    /// @note  The destructor of `T` is skipped when `T` is trivially destructible.
    ~Optional() { reset(); }

    /// @brief Copy-assignment operator.
    /// @param o  Source `Optional` to copy from.
    /// @return   Reference to `*this`.
    Optional& operator=(const Optional& o) {
        if (this != &o) { reset(); has_ = o.has_; if (has_) new (storage_) T(*o.ptr()); }
        return *this;
    }

    /// @brief Move-assignment operator.
    /// @param o  Source `Optional` to move from; left empty after the call.
    /// @return   Reference to `*this`.
    Optional& operator=(Optional&& o) noexcept {
        if (this != &o) {
            reset(); has_ = o.has_;
            if (has_) { new (storage_) T(traits::move(*o.ptr())); o.reset(); }
        }
        return *this;
    }

    /// @brief Destroys the contained value (if any) and marks the `Optional` as empty.
    /// @post  `has_value()` returns `false`.
    /// @note  If `T` is trivially destructible, the destructor call is elided.
    void reset() noexcept {
        if (has_) {
            if constexpr (!traits::is_trivially_destructible_v<T>) ptr()->~T();
            has_ = false;
        }
    }

    /// @brief Destroys any current value, then constructs a new value in-place from `args`.
    /// @tparam Args  Constructor argument types forwarded to `T`'s constructor.
    /// @param  args  Arguments forwarded to `T`'s constructor.
    /// @return Reference to the newly constructed contained value.
    /// @post   `has_value()` returns `true`.
    template<typename... Args>
    T& emplace(Args&&... args) {
        reset();
        new (storage_) T(traits::forward<Args>(args)...);
        has_ = true;
        return *ptr();
    }

    /// @brief Returns `true` if the `Optional` holds a value.
    /// @return `true` when a value is present, `false` otherwise.
    [[nodiscard]] bool has_value()          const noexcept { return has_; }

    /// @brief Explicit boolean conversion — `true` when a value is present.
    /// @return `true` when a value is present, `false` otherwise.
    [[nodiscard]] explicit operator bool()  const noexcept { return has_; }

    /// @brief Returns a mutable reference to the contained value.
    /// @return Mutable reference to `T`.
    /// @pre   `has_value()` must be `true`.
    /// @warning Calling this on an empty `Optional` is undefined behaviour.
    T&       value()       noexcept { return *ptr(); }

    /// @brief Returns a const reference to the contained value.
    /// @return Const reference to `T`.
    /// @pre   `has_value()` must be `true`.
    /// @warning Calling this on an empty `Optional` is undefined behaviour.
    const T& value() const noexcept { return *ptr(); }

    /// @brief Dereference operator — returns a mutable reference to the contained value.
    /// @return Mutable reference to `T`.
    /// @pre   `has_value()` must be `true`.
    /// @warning Dereferencing an empty `Optional` is undefined behaviour.
    T&       operator*()       noexcept { return *ptr(); }

    /// @brief Dereference operator — returns a const reference to the contained value.
    /// @return Const reference to `T`.
    /// @pre   `has_value()` must be `true`.
    /// @warning Dereferencing an empty `Optional` is undefined behaviour.
    const T& operator*() const noexcept { return *ptr(); }

    /// @brief Arrow operator — returns a mutable pointer to the contained value.
    /// @return Mutable pointer to `T`.
    /// @pre   `has_value()` must be `true`.
    /// @warning Using this on an empty `Optional` is undefined behaviour.
    T*       operator->()      noexcept { return ptr(); }

    /// @brief Arrow operator — returns a const pointer to the contained value.
    /// @return Const pointer to `T`.
    /// @pre   `has_value()` must be `true`.
    /// @warning Using this on an empty `Optional` is undefined behaviour.
    const T* operator->() const noexcept { return ptr(); }

    /// @brief Returns the contained value, or `fallback` if empty.
    /// @param fallback  Value returned when the `Optional` is empty.
    /// @return Copy of the contained value, or `fallback`.
    T value_or(T fallback) const {
        return has_ ? *ptr() : traits::move(fallback);
    }

    /// @brief Equality comparison — two `Optional`s are equal when both are empty
    ///        or both contain equal values.
    /// @param o  Right-hand side `Optional` to compare with.
    /// @return   `true` if both have the same engagement state and equal values.
    bool operator==(const Optional& o) const noexcept {
        if (has_ != o.has_) return false;
        if (!has_) return true;
        return *ptr() == *o.ptr();
    }

    /// @brief Inequality comparison.
    /// @param o  Right-hand side `Optional` to compare with.
    /// @return   `true` if the two `Optional`s are not equal.
    bool operator!=(const Optional& o) const noexcept { return !(*this == o); }
};

// Convenience factories

/// @brief Creates an engaged `Optional` holding a copy/move of `v`.
/// @tparam T  Deduced value type; cv-qualifiers and references are decayed.
/// @param  v  Value to store; forwarded into the `Optional`.
/// @return    An engaged `Optional<decay_t<T>>`.
template<typename T>
[[nodiscard]] Optional<traits::decay_t<T>> some(T&& v) {
    return Optional<traits::decay_t<T>>(traits::forward<T>(v));
}

/// @brief Creates an empty (valueless) `Optional<T>`.
/// @tparam T  The value type the resulting `Optional` would hold.
/// @return    An empty `Optional<T>`.
template<typename T>
[[nodiscard]] Optional<T> none_of() { return Optional<T>{}; }

} // namespace dsc
