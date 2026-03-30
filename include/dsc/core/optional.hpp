#pragma once

#include "primitives.hpp"
#include "traits.hpp"

namespace dsc {

// ── Optional<T> ───────────────────────────────────────────────────────────────
// Lightweight nullable value container — replaces std::optional.
// Uses aligned storage so T is never default-constructed unless explicitly set.
template<typename T>
class Optional {
    alignas(T) unsigned char storage_[sizeof(T)];
    bool has_;

    T*       ptr()       noexcept { return reinterpret_cast<T*>(storage_); }
    const T* ptr() const noexcept { return reinterpret_cast<const T*>(storage_); }

public:
    constexpr Optional() noexcept : storage_{}, has_(false) {}

    Optional(const T& v) noexcept(traits::is_trivially_constructible_v<T, const T&>)
        : has_(true) { new (storage_) T(v); }

    Optional(T&& v) noexcept(traits::is_trivially_constructible_v<T, T&&>)
        : has_(true) { new (storage_) T(traits::move(v)); }

    Optional(const Optional& o) : has_(o.has_) {
        if (has_) new (storage_) T(*o.ptr());
    }

    Optional(Optional&& o) noexcept : has_(o.has_) {
        if (has_) {
            new (storage_) T(traits::move(*o.ptr()));
            o.reset();
        }
    }

    ~Optional() { reset(); }

    Optional& operator=(const Optional& o) {
        if (this != &o) { reset(); has_ = o.has_; if (has_) new (storage_) T(*o.ptr()); }
        return *this;
    }

    Optional& operator=(Optional&& o) noexcept {
        if (this != &o) {
            reset(); has_ = o.has_;
            if (has_) { new (storage_) T(traits::move(*o.ptr())); o.reset(); }
        }
        return *this;
    }

    void reset() noexcept {
        if (has_) {
            if constexpr (!traits::is_trivially_destructible_v<T>) ptr()->~T();
            has_ = false;
        }
    }

    template<typename... Args>
    T& emplace(Args&&... args) {
        reset();
        new (storage_) T(traits::forward<Args>(args)...);
        has_ = true;
        return *ptr();
    }

    [[nodiscard]] bool has_value()          const noexcept { return has_; }
    [[nodiscard]] explicit operator bool()  const noexcept { return has_; }

    T&       value()       noexcept { return *ptr(); }
    const T& value() const noexcept { return *ptr(); }

    T&       operator*()       noexcept { return *ptr(); }
    const T& operator*() const noexcept { return *ptr(); }
    T*       operator->()      noexcept { return ptr(); }
    const T* operator->() const noexcept { return ptr(); }

    T value_or(T fallback) const {
        return has_ ? *ptr() : traits::move(fallback);
    }

    bool operator==(const Optional& o) const noexcept {
        if (has_ != o.has_) return false;
        if (!has_) return true;
        return *ptr() == *o.ptr();
    }
    bool operator!=(const Optional& o) const noexcept { return !(*this == o); }
};

// Convenience factories
template<typename T>
[[nodiscard]] Optional<traits::decay_t<T>> some(T&& v) {
    return Optional<traits::decay_t<T>>(traits::forward<T>(v));
}

template<typename T>
[[nodiscard]] Optional<T> none_of() { return Optional<T>{}; }

} // namespace dsc
