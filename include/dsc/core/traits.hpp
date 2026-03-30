#pragma once

#include "primitives.hpp"

// Hand-rolled type traits — no <type_traits> from the STL.
// We rely only on compiler built-ins (__is_trivially_copyable, etc.)
// that GCC ≥ 4.8, Clang ≥ 3.0, and MSVC 2015+ all provide natively.

/// @brief Sub-namespace containing hand-rolled type-traits utilities for the dsc library.
/// @note  No `<type_traits>` header is included.  Trivial-property detection
///        relies solely on compiler built-ins (`__is_trivially_copyable`, etc.)
///        available on GCC >= 4.8, Clang >= 3.0, and MSVC 2015+.
namespace dsc::traits {

// ── integral_constant ────────────────────────────────────────────────────────

/// @brief Compile-time constant wrapper that holds a value of type `T`.
/// @tparam T  Type of the constant value.
/// @tparam V  The constant value itself.
template<typename T, T V>
struct integral_constant {
    /// @brief The wrapped constant value.
    static constexpr T value = V;
    /// @brief The type of the wrapped value.
    using value_type = T;
    /// @brief Implicit conversion to `T`; returns `V`.
    /// @return The compile-time constant `V`.
    constexpr operator T() const noexcept { return V; }
};

/// @brief Convenience alias for `integral_constant<bool, true>`.
using true_type  = integral_constant<bool, true>;
/// @brief Convenience alias for `integral_constant<bool, false>`.
using false_type = integral_constant<bool, false>;

// ── remove_cv ────────────────────────────────────────────────────────────────

/// @brief Removes the `const` qualifier from a type.
/// @tparam T  The (possibly const-qualified) input type.
template<typename T> struct remove_const          { using type = T; };
/// @brief Specialisation that strips `const` from `const T`.
/// @tparam T  The underlying type after removing `const`.
template<typename T> struct remove_const<const T> { using type = T; };

/// @brief Removes the `volatile` qualifier from a type.
/// @tparam T  The (possibly volatile-qualified) input type.
template<typename T> struct remove_volatile             { using type = T; };
/// @brief Specialisation that strips `volatile` from `volatile T`.
/// @tparam T  The underlying type after removing `volatile`.
template<typename T> struct remove_volatile<volatile T> { using type = T; };

/// @brief Removes both `const` and `volatile` qualifiers from a type.
/// @tparam T  The (possibly cv-qualified) input type.
template<typename T>
struct remove_cv { using type = typename remove_const<typename remove_volatile<T>::type>::type; };

/// @brief Helper alias that yields `T` stripped of all cv-qualifiers.
/// @tparam T  The (possibly cv-qualified) input type.
template<typename T> using remove_cv_t = typename remove_cv<T>::type;

// ── remove_ref ───────────────────────────────────────────────────────────────

/// @brief Removes reference qualifiers from a type.
/// @tparam T  The input type (no reference, lvalue reference, or rvalue reference).
template<typename T> struct remove_ref      { using type = T; };
/// @brief Specialisation that strips an lvalue reference from `T&`.
/// @tparam T  The referenced type.
template<typename T> struct remove_ref<T&>  { using type = T; };
/// @brief Specialisation that strips an rvalue reference from `T&&`.
/// @tparam T  The referenced type.
template<typename T> struct remove_ref<T&&> { using type = T; };
/// @brief Helper alias that yields `T` with any reference qualifier removed.
/// @tparam T  The input type.
template<typename T> using remove_ref_t = typename remove_ref<T>::type;

// ── decay ─────────────────────────────────────────────────────────────────────

/// @brief Applies the "decay" transformation: removes references then cv-qualifiers.
/// @tparam T  The input type (may be a reference or cv-qualified).
/// @note  Array-to-pointer and function-to-pointer decay are not implemented
///        here as they are not required by the dsc containers.
template<typename T>
struct decay { using type = remove_cv_t<remove_ref_t<T>>; };
/// @brief Helper alias that yields the decayed form of `T`.
/// @tparam T  The input type.
template<typename T> using decay_t = typename decay<T>::type;

// ── is_same ───────────────────────────────────────────────────────────────────

/// @brief Checks whether two types are identical (ignoring nothing — exact match).
/// @tparam A  First type.
/// @tparam B  Second type.
template<typename A, typename B> struct is_same       : false_type {};
/// @brief Specialisation that matches when both type parameters are the same.
/// @tparam A  The single type that appears on both sides.
template<typename A>             struct is_same<A, A> : true_type  {};
/// @brief Variable-template shorthand for `is_same<A,B>::value`.
/// @tparam A  First type.
/// @tparam B  Second type.
template<typename A, typename B> inline constexpr bool is_same_v = is_same<A, B>::value;

// ── conditional ───────────────────────────────────────────────────────────────

/// @brief Selects one of two types at compile time based on a boolean condition.
/// @tparam Cond  If `true`, `type` aliases `T`; if `false`, `type` aliases `F`.
/// @tparam T     Type selected when `Cond` is `true`.
/// @tparam F     Type selected when `Cond` is `false`.
template<bool Cond, typename T, typename F> struct conditional           { using type = T; };
/// @brief Specialisation for `Cond == false`: selects `F`.
/// @tparam T  Unused (false-branch discards this type).
/// @tparam F  The type aliased as `type` when the condition is false.
template<         typename T, typename F>   struct conditional<false,T,F>{ using type = F; };
/// @brief Helper alias that yields `T` or `F` based on compile-time condition `C`.
/// @tparam C  Boolean selector.
/// @tparam T  Type when `C` is `true`.
/// @tparam F  Type when `C` is `false`.
template<bool C, typename T, typename F> using conditional_t = typename conditional<C,T,F>::type;

// ── enable_if ─────────────────────────────────────────────────────────────────

/// @brief SFINAE helper: provides `type = T` only when `Cond` is `true`.
/// @tparam Cond  Boolean condition that enables or disables the member typedef.
/// @tparam T     The type exposed as `type`; defaults to `void`.
template<bool, typename T = void> struct enable_if {};
/// @brief Specialisation for `Cond == true`: exposes `type = T`.
/// @tparam T  The type to expose.
template<typename T>              struct enable_if<true, T> { using type = T; };
/// @brief Helper alias for `enable_if<C,T>::type`.
/// @tparam C  Boolean enabling condition.
/// @tparam T  Result type (defaults to `void`).
template<bool C, typename T = void> using enable_if_t = typename enable_if<C,T>::type;

// ── Trivial-type detection (compiler built-ins) ───────────────────────────────

/// @brief Checks whether `T` is trivially copyable (via compiler built-in).
/// @tparam T  Type to query.
template<typename T> struct is_trivially_copyable
    : integral_constant<bool, __is_trivially_copyable(T)> {};
/// @brief Variable-template shorthand for `is_trivially_copyable<T>::value`.
/// @tparam T  Type to query.
template<typename T> inline constexpr bool is_trivially_copyable_v =
    is_trivially_copyable<T>::value;

/// @brief Checks whether `T` is trivially destructible (via compiler built-in).
/// @tparam T  Type to query.
template<typename T> struct is_trivially_destructible
    : integral_constant<bool, __is_trivially_destructible(T)> {};
/// @brief Variable-template shorthand for `is_trivially_destructible<T>::value`.
/// @tparam T  Type to query.
template<typename T> inline constexpr bool is_trivially_destructible_v =
    is_trivially_destructible<T>::value;

/// @brief Checks whether `T` is trivially constructible from `Args...` (via compiler built-in).
/// @tparam T     Type to construct.
/// @tparam Args  Constructor argument types.
template<typename T, typename... Args> struct is_trivially_constructible
    : integral_constant<bool, __is_trivially_constructible(T, Args...)> {};
/// @brief Variable-template shorthand for `is_trivially_constructible<T,Args...>::value`.
/// @tparam T     Type to construct.
/// @tparam Args  Constructor argument types.
template<typename T, typename... Args> inline constexpr bool is_trivially_constructible_v =
    is_trivially_constructible<T, Args...>::value;

// ── Integer type checks ───────────────────────────────────────────────────────

/// @brief Checks whether `T` (cv-unqualified) is an integral type.
/// @tparam T  Type to query.
/// @note  cv-qualifiers are stripped before the check via `remove_cv_t`.
template<typename T> struct is_integral : false_type {};
/// @cond SPECIALISATIONS
template<> struct is_integral<bool>               : true_type {};
template<> struct is_integral<char>               : true_type {};
template<> struct is_integral<signed char>        : true_type {};
template<> struct is_integral<unsigned char>      : true_type {};
template<> struct is_integral<short>              : true_type {};
template<> struct is_integral<unsigned short>     : true_type {};
template<> struct is_integral<int>                : true_type {};
template<> struct is_integral<unsigned int>       : true_type {};
template<> struct is_integral<long>               : true_type {};
template<> struct is_integral<unsigned long>      : true_type {};
template<> struct is_integral<long long>          : true_type {};
template<> struct is_integral<unsigned long long> : true_type {};
/// @endcond
/// @brief Variable-template shorthand for `is_integral<remove_cv_t<T>>::value`.
/// @tparam T  Type to query (cv-qualifiers are ignored).
template<typename T> inline constexpr bool is_integral_v = is_integral<remove_cv_t<T>>::value;

/// @brief Checks whether `T` (cv-unqualified) is an unsigned integral type.
/// @tparam T  Type to query.
/// @note  cv-qualifiers are stripped before the check via `remove_cv_t`.
template<typename T> struct is_unsigned : false_type {};
/// @cond SPECIALISATIONS
template<> struct is_unsigned<unsigned char>      : true_type {};
template<> struct is_unsigned<unsigned short>     : true_type {};
template<> struct is_unsigned<unsigned int>       : true_type {};
template<> struct is_unsigned<unsigned long>      : true_type {};
template<> struct is_unsigned<unsigned long long> : true_type {};
/// @endcond
/// @brief Variable-template shorthand for `is_unsigned<remove_cv_t<T>>::value`.
/// @tparam T  Type to query (cv-qualifiers are ignored).
template<typename T> inline constexpr bool is_unsigned_v = is_unsigned<remove_cv_t<T>>::value;

// ── Pointer check ─────────────────────────────────────────────────────────────

/// @brief Checks whether `T` (cv-unqualified) is a pointer type.
/// @tparam T  Type to query.
template<typename T> struct is_pointer      : false_type {};
/// @brief Specialisation that matches `T*` (pointer-to-T).
/// @tparam T  Pointee type.
template<typename T> struct is_pointer<T*>  : true_type  {};
/// @brief Variable-template shorthand for `is_pointer<remove_cv_t<T>>::value`.
/// @tparam T  Type to query (cv-qualifiers are ignored).
template<typename T> inline constexpr bool is_pointer_v = is_pointer<remove_cv_t<T>>::value;

// ── forward / move ────────────────────────────────────────────────────────────

/// @brief Perfect-forward an lvalue as either an lvalue or rvalue reference.
/// @tparam T  Deduced forwarding reference type.
/// @param  t  Lvalue reference to forward.
/// @return    `static_cast<T&&>(t)`.
template<typename T>
[[nodiscard]] constexpr T&& forward(remove_ref_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}
/// @brief Perfect-forward an rvalue as an rvalue reference.
/// @tparam T  Deduced forwarding reference type.
/// @param  t  Rvalue reference to forward.
/// @return    `static_cast<T&&>(t)`.
/// @warning   Forwarding an lvalue via this overload is ill-formed.
template<typename T>
[[nodiscard]] constexpr T&& forward(remove_ref_t<T>&& t) noexcept {
    return static_cast<T&&>(t);
}

/// @brief Cast `t` to an rvalue reference, enabling move semantics.
/// @tparam T  Type of the argument (deduced; references are stripped for the return type).
/// @param  t  Object to move from.
/// @return    `static_cast<remove_ref_t<T>&&>(t)`.
template<typename T>
[[nodiscard]] constexpr remove_ref_t<T>&& move(T&& t) noexcept {
    return static_cast<remove_ref_t<T>&&>(t);
}

// ── swap ─────────────────────────────────────────────────────────────────────

/// @brief Exchange the values of `a` and `b` using move semantics.
/// @tparam T  Type of the objects to swap; must be move-constructible and move-assignable.
/// @param  a  First object; holds `b`'s value after the call.
/// @param  b  Second object; holds `a`'s value after the call.
/// @complexity O(1)
template<typename T>
constexpr void swap(T& a, T& b) noexcept {
    T tmp = move(a);
    a = move(b);
    b = move(tmp);
}

} // namespace dsc::traits

// Make forward/move/swap accessible in dsc:: directly
namespace dsc {
    using traits::forward;
    using traits::move;
    using traits::swap;
}
