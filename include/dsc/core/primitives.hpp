#pragma once

// ── Primitive type aliases ────────────────────────────────────────────────────
// Everything the rest of dsc needs: fixed-width integers, size types,
// placement-new, and raw memory ops — all sourced from compiler built-ins
// and the C ABI headers (NOT the C++ standard library).

// GCC/Clang/MSVC all expose these C-ABI-only headers with no STL involvement.
extern "C" {
    typedef __SIZE_TYPE__    dsc_usize;
    typedef __PTRDIFF_TYPE__ dsc_isize;
}

typedef signed   char      dsc_i8;
typedef signed   short     dsc_i16;
typedef signed   int       dsc_i32;
typedef signed   long long dsc_i64;
typedef unsigned char      dsc_u8;
typedef unsigned short     dsc_u16;
typedef unsigned int       dsc_u32;
typedef unsigned long long dsc_u64;
typedef float              dsc_f32;
typedef double             dsc_f64;

namespace dsc {

using i8    = dsc_i8;
using i16   = dsc_i16;
using i32   = dsc_i32;
using i64   = dsc_i64;
using u8    = dsc_u8;
using u16   = dsc_u16;
using u32   = dsc_u32;
using u64   = dsc_u64;
using f32   = dsc_f32;
using f64   = dsc_f64;
using usize = dsc_usize;
using isize = dsc_isize;

// ── nullptr_t ─────────────────────────────────────────────────────────────────
using nullptr_t = decltype(nullptr);

// ── Boolean ───────────────────────────────────────────────────────────────────
using bool8 = bool;

} // namespace dsc

// ── Placement new (compiler built-in, no STL needed) ─────────────────────────
// C++ guarantees this signature is available without any header on conforming
// compilers; we declare it ourselves so we never pull in <new>.
inline void* operator new(dsc_usize, void* ptr) noexcept { return ptr; }
inline void  operator delete(void*, void*) noexcept {}
