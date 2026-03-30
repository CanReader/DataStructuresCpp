#pragma once

#include "primitives.hpp"

// Hand-rolled type traits — no <type_traits> from the STL.
// We rely only on compiler built-ins (__is_trivially_copyable, etc.)
// that GCC ≥ 4.8, Clang ≥ 3.0, and MSVC 2015+ all provide natively.

namespace dsc::traits {

// ── integral_constant ────────────────────────────────────────────────────────
template<typename T, T V>
struct integral_constant {
    static constexpr T value = V;
    using value_type = T;
    constexpr operator T() const noexcept { return V; }
};

using true_type  = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

// ── remove_cv ────────────────────────────────────────────────────────────────
template<typename T> struct remove_const          { using type = T; };
template<typename T> struct remove_const<const T> { using type = T; };

template<typename T> struct remove_volatile             { using type = T; };
template<typename T> struct remove_volatile<volatile T> { using type = T; };

template<typename T>
struct remove_cv { using type = typename remove_const<typename remove_volatile<T>::type>::type; };

template<typename T> using remove_cv_t = typename remove_cv<T>::type;

// ── remove_ref ───────────────────────────────────────────────────────────────
template<typename T> struct remove_ref      { using type = T; };
template<typename T> struct remove_ref<T&>  { using type = T; };
template<typename T> struct remove_ref<T&&> { using type = T; };
template<typename T> using remove_ref_t = typename remove_ref<T>::type;

// ── decay ─────────────────────────────────────────────────────────────────────
template<typename T>
struct decay { using type = remove_cv_t<remove_ref_t<T>>; };
template<typename T> using decay_t = typename decay<T>::type;

// ── is_same ───────────────────────────────────────────────────────────────────
template<typename A, typename B> struct is_same       : false_type {};
template<typename A>             struct is_same<A, A> : true_type  {};
template<typename A, typename B> inline constexpr bool is_same_v = is_same<A, B>::value;

// ── conditional ───────────────────────────────────────────────────────────────
template<bool Cond, typename T, typename F> struct conditional           { using type = T; };
template<         typename T, typename F>   struct conditional<false,T,F>{ using type = F; };
template<bool C, typename T, typename F> using conditional_t = typename conditional<C,T,F>::type;

// ── enable_if ─────────────────────────────────────────────────────────────────
template<bool, typename T = void> struct enable_if {};
template<typename T>              struct enable_if<true, T> { using type = T; };
template<bool C, typename T = void> using enable_if_t = typename enable_if<C,T>::type;

// ── Trivial-type detection (compiler built-ins) ───────────────────────────────
template<typename T> struct is_trivially_copyable
    : integral_constant<bool, __is_trivially_copyable(T)> {};
template<typename T> inline constexpr bool is_trivially_copyable_v =
    is_trivially_copyable<T>::value;

template<typename T> struct is_trivially_destructible
    : integral_constant<bool, __is_trivially_destructible(T)> {};
template<typename T> inline constexpr bool is_trivially_destructible_v =
    is_trivially_destructible<T>::value;

template<typename T, typename... Args> struct is_trivially_constructible
    : integral_constant<bool, __is_trivially_constructible(T, Args...)> {};
template<typename T, typename... Args> inline constexpr bool is_trivially_constructible_v =
    is_trivially_constructible<T, Args...>::value;

// ── Integer type checks ───────────────────────────────────────────────────────
template<typename T> struct is_integral : false_type {};
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
template<typename T> inline constexpr bool is_integral_v = is_integral<remove_cv_t<T>>::value;

template<typename T> struct is_unsigned : false_type {};
template<> struct is_unsigned<unsigned char>      : true_type {};
template<> struct is_unsigned<unsigned short>     : true_type {};
template<> struct is_unsigned<unsigned int>       : true_type {};
template<> struct is_unsigned<unsigned long>      : true_type {};
template<> struct is_unsigned<unsigned long long> : true_type {};
template<typename T> inline constexpr bool is_unsigned_v = is_unsigned<remove_cv_t<T>>::value;

// ── Pointer check ─────────────────────────────────────────────────────────────
template<typename T> struct is_pointer      : false_type {};
template<typename T> struct is_pointer<T*>  : true_type  {};
template<typename T> inline constexpr bool is_pointer_v = is_pointer<remove_cv_t<T>>::value;

// ── forward / move ────────────────────────────────────────────────────────────
template<typename T>
[[nodiscard]] constexpr T&& forward(remove_ref_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}
template<typename T>
[[nodiscard]] constexpr T&& forward(remove_ref_t<T>&& t) noexcept {
    return static_cast<T&&>(t);
}

template<typename T>
[[nodiscard]] constexpr remove_ref_t<T>&& move(T&& t) noexcept {
    return static_cast<remove_ref_t<T>&&>(t);
}

// ── swap ─────────────────────────────────────────────────────────────────────
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
