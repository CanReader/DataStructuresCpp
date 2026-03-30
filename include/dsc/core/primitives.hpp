#pragma once

// ── Primitive type aliases ────────────────────────────────────────────────────
// Everything the rest of dsc needs: fixed-width integers, size types,
// placement-new, and raw memory ops — all sourced from compiler built-ins
// and the C ABI headers (NOT the C++ standard library).

// GCC/Clang/MSVC all expose these C-ABI-only headers with no STL involvement.

/// @brief Unsigned platform-native size type sourced from the compiler built-in `__SIZE_TYPE__`.
/// @note Equivalent to `size_t` but declared without any STL or libc header.
extern "C" {
    typedef __SIZE_TYPE__    dsc_usize;
    /// @brief Signed pointer-difference type sourced from the compiler built-in `__PTRDIFF_TYPE__`.
    /// @note Equivalent to `ptrdiff_t` but declared without any STL or libc header.
    typedef __PTRDIFF_TYPE__ dsc_isize;
}

/// @brief 8-bit signed integer (C-linkage typedef).
typedef signed   char      dsc_i8;
/// @brief 16-bit signed integer (C-linkage typedef).
typedef signed   short     dsc_i16;
/// @brief 32-bit signed integer (C-linkage typedef).
typedef signed   int       dsc_i32;
/// @brief 64-bit signed integer (C-linkage typedef).
typedef signed   long long dsc_i64;
/// @brief 8-bit unsigned integer (C-linkage typedef).
typedef unsigned char      dsc_u8;
/// @brief 16-bit unsigned integer (C-linkage typedef).
typedef unsigned short     dsc_u16;
/// @brief 32-bit unsigned integer (C-linkage typedef).
typedef unsigned int       dsc_u32;
/// @brief 64-bit unsigned integer (C-linkage typedef).
typedef unsigned long long dsc_u64;
/// @brief 32-bit IEEE 754 single-precision floating-point (C-linkage typedef).
typedef float              dsc_f32;
/// @brief 64-bit IEEE 754 double-precision floating-point (C-linkage typedef).
typedef double             dsc_f64;

/// @brief Root namespace for the dsc zero-dependency data-structures library.
namespace dsc {

/// @brief 8-bit signed integer alias.
using i8    = dsc_i8;
/// @brief 16-bit signed integer alias.
using i16   = dsc_i16;
/// @brief 32-bit signed integer alias.
using i32   = dsc_i32;
/// @brief 64-bit signed integer alias.
using i64   = dsc_i64;
/// @brief 8-bit unsigned integer alias.
using u8    = dsc_u8;
/// @brief 16-bit unsigned integer alias.
using u16   = dsc_u16;
/// @brief 32-bit unsigned integer alias.
using u32   = dsc_u32;
/// @brief 64-bit unsigned integer alias.
using u64   = dsc_u64;
/// @brief 32-bit single-precision floating-point alias.
using f32   = dsc_f32;
/// @brief 64-bit double-precision floating-point alias.
using f64   = dsc_f64;
/// @brief Unsigned size type whose width equals a native pointer (platform-dependent).
using usize = dsc_usize;
/// @brief Signed size/difference type whose width equals a native pointer (platform-dependent).
using isize = dsc_isize;

// ── nullptr_t ─────────────────────────────────────────────────────────────────
/// @brief Alias for the type of `nullptr`, obtained without `<cstddef>`.
using nullptr_t = decltype(nullptr);

// ── Boolean ───────────────────────────────────────────────────────────────────
/// @brief Boolean type alias (`bool`).
using bool8 = bool;

} // namespace dsc

// ── Placement new (compiler built-in, no STL needed) ─────────────────────────
// C++ guarantees this signature is available without any header on conforming
// compilers; we declare it ourselves so we never pull in <new>.

/// @brief Placement `operator new` — returns `ptr` unchanged, constructing nothing.
/// @param   (unnamed) Requested allocation size; ignored by placement new.
/// @param   ptr       Pre-allocated memory region to construct the object into.
/// @return  `ptr` unchanged.
/// @note    Declared explicitly here so that `<new>` is never included.
///          The C++ standard guarantees this signature on all conforming compilers.
inline void* operator new(dsc_usize, void* ptr) noexcept { return ptr; }

/// @brief Matching placement `operator delete` — a required no-op.
/// @param (unnamed) Object pointer; memory is not freed.
/// @param (unnamed) Original placement address; ignored.
/// @note  The C++ standard requires this overload to exist alongside the
///        placement `operator new` above; it performs no action.
inline void  operator delete(void*, void*) noexcept {}
