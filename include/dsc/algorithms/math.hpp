#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../containers/array.hpp"
#include "../containers/bitset.hpp"

// ── Mathematical algorithms ───────────────────────────────────────────────────
// Number theory, combinatorics, matrix operations — no math.h needed.

namespace dsc {

// ── GCD / LCM ─────────────────────────────────────────────────────────────────
/// @brief Computes the greatest common divisor of two integers.
/// @tparam T Integer type.
/// @param a First integer.
/// @param b Second integer.
/// @return Greatest common divisor of a and b.
/// @complexity O(log min(a, b)).
template<typename T>
[[nodiscard]] constexpr T gcd(T a, T b) noexcept {
    while (b) { a %= b; T t = a; a = b; b = t; }
    return a;
}

/// @brief Computes the least common multiple of two integers.
/// @tparam T Integer type.
/// @param a First integer.
/// @param b Second integer.
/// @return Least common multiple of a and b.
/// @complexity O(log min(a, b)).
template<typename T>
[[nodiscard]] constexpr T lcm(T a, T b) noexcept { return a / gcd(a, b) * b; }

// ── Fast integer power ────────────────────────────────────────────────────────
// a^b using binary exponentiation. O(log b).
/// @brief Computes a raised to the power b using binary exponentiation.
/// @tparam T Arithmetic type.
/// @param a Base value.
/// @param b Exponent (non-negative).
/// @return a^b.
/// @complexity O(log b).
template<typename T>
[[nodiscard]] constexpr T pow_int(T a, u64 b) noexcept {
    T result = T(1);
    while (b) {
        if (b & 1) result *= a;
        a *= a;
        b >>= 1;
    }
    return result;
}

// Modular fast power: (a^b) % mod. O(log b).
/// @brief Computes (a^b) mod mod using binary exponentiation.
/// @param a Base value.
/// @param b Exponent.
/// @param mod Modulus.
/// @return (a^b) % mod.
/// @complexity O(log b).
[[nodiscard]] constexpr u64 pow_mod(u64 a, u64 b, u64 mod) noexcept {
    u64 result = 1;
    a %= mod;
    while (b) {
        if (b & 1) result = result * a % mod;
        a = a * a % mod;
        b >>= 1;
    }
    return result;
}

// ── Modular inverse ───────────────────────────────────────────────────────────
// Modular inverse of a under prime modulus p: a^(p-2) % p (Fermat's little theorem).
/// @brief Computes the modular inverse of a modulo p using Fermat's little theorem.
/// @param a Value to find inverse for.
/// @param p Prime modulus.
/// @return Modular inverse of a modulo p.
/// @pre p is prime and gcd(a, p) = 1.
/// @complexity O(log p).
[[nodiscard]] constexpr u64 mod_inverse(u64 a, u64 p) noexcept {
    return pow_mod(a, p - 2, p);
}

// Extended Euclidean algorithm: returns gcd(a, b) and sets x, y s.t. ax + by = gcd
/// @brief Computes the extended GCD and Bézout coefficients.
/// @tparam T Integer type.
/// @param a First integer.
/// @param b Second integer.
/// @param x Output: coefficient for a.
/// @param y Output: coefficient for b.
/// @return Greatest common divisor of a and b.
/// @note Finds x, y such that a*x + b*y = gcd(a, b).
template<typename T>
T extended_gcd(T a, T b, T& x, T& y) noexcept {
    if (b == 0) { x = 1; y = 0; return a; }
    T x1, y1;
    T g = extended_gcd(b, a % b, x1, y1);
    x = y1;
    y = x1 - (a / b) * y1;
    return g;
}

// ── Sieve of Eratosthenes ─────────────────────────────────────────────────────
// Returns array of all primes in [2, n]. O(n log log n).
/// @brief Generates all prime numbers up to n using the Sieve of Eratosthenes.
/// @param n Upper limit (inclusive).
/// @return Array of all prime numbers from 2 to n.
/// @complexity O(n log log n).
inline Array<u32> sieve_primes(u32 n) {
    DynBitSet composite(n + 1, false);
    Array<u32> primes;
    if (n < 2) return primes;

    composite.set(0); composite.set(1);
    for (u64 i = 2; (u64)i * i <= n; ++i) {
        if (!composite.test(i)) {
            for (u64 j = (u64)i * i; j <= n; j += i)
                composite.set(static_cast<usize>(j));
        }
    }
    for (u32 i = 2; i <= n; ++i)
        if (!composite.test(i)) primes.push_back(i);
    return primes;
}

// Segmented sieve for large n — O(n log log n), O(sqrt(n)) memory
/// @brief Generates prime numbers in the range [lo, hi] using segmented sieve.
/// @param lo Lower bound (inclusive).
/// @param hi Upper bound (inclusive).
/// @return Array of prime numbers from lo to hi.
/// @complexity O((hi - lo) log log hi).
inline Array<u64> segmented_sieve(u64 lo, u64 hi) {
    // Find primes up to sqrt(hi)
    u64 sq = 1; while (sq * sq <= hi) ++sq;
    Array<u32> small = sieve_primes(static_cast<u32>(sq));

    Array<u64> result;
    usize seg_size = static_cast<usize>(hi - lo + 1);
    DynBitSet is_prime(seg_size, true);

    // 0 and 1 are not prime
    if (lo == 0 && seg_size > 0) is_prime.reset(0);
    if (lo <= 1 && 1 - lo < seg_size) is_prime.reset(static_cast<usize>(1 - lo));

    for (u32 p : small) {
        u64 start = ((lo + p - 1) / p) * p;
        if (start == p) start += p;  // skip p itself
        for (u64 j = start; j <= hi; j += p)
            is_prime.reset(static_cast<usize>(j - lo));
    }

    for (usize i = 0; i < seg_size; ++i)
        if (is_prime.test(i)) result.push_back(lo + i);
    return result;
}

// ── Miller-Rabin primality test ───────────────────────────────────────────────
// Deterministic for n < 3,215,031,751 using bases {2,3,5,7}.
// Probabilistic for larger n with the same bases (very low false-positive rate).
/// @brief Tests if a number is prime using Miller-Rabin primality test.
/// @param n Number to test.
/// @return true if n is prime, false otherwise.
/// @note Deterministic for n < 3,215,031,751; probabilistic for larger n.
inline bool is_prime(u64 n) noexcept {
    if (n < 2) return false;
    if (n == 2 || n == 3 || n == 5 || n == 7) return true;
    if (n % 2 == 0 || n % 3 == 0 || n % 5 == 0) return false;

    // Write n-1 as 2^r * d
    u64 d = n - 1; u32 r = 0;
    while (!(d & 1)) { d >>= 1; ++r; }

    // Test with these witnesses — deterministic for n < 3,215,031,751
    for (u64 a : {2ULL, 3ULL, 5ULL, 7ULL}) {
        if (a >= n) continue;
        u64 x = pow_mod(a, d, n);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (u32 i = 0; i + 1 < r; ++i) {
            x = x * x % n;
            if (x == n - 1) { composite = false; break; }
        }
        if (composite) return false;
    }
    return true;
}

// ── Integer square root ───────────────────────────────────────────────────────
/// @brief Computes the integer square root of n (floor(sqrt(n))).
/// @param n Non-negative integer.
/// @return Largest integer x such that x*x <= n.
/// @complexity O(log n).
[[nodiscard]] constexpr u64 isqrt(u64 n) noexcept {
    if (n == 0) return 0;
    u64 x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

// ── Absolute value ────────────────────────────────────────────────────────────
/// @brief Computes the absolute value of x.
/// @tparam T Arithmetic type.
/// @param x Value to take absolute value of.
/// @return Absolute value of x.
template<typename T>
[[nodiscard]] constexpr T abs_val(T x) noexcept { return x < T(0) ? -x : x; }

// ── Integer log2 floor ────────────────────────────────────────────────────────
/// @brief Computes floor(log2(n)) for n > 0.
/// @param n Positive integer.
/// @return Floor of base-2 logarithm of n.
/// @pre n > 0.
[[nodiscard]] constexpr u32 ilog2(u64 n) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return n == 0 ? 0 : 63 - static_cast<u32>(__builtin_clzll(n));
#else
    u32 r = 0; while (n > 1) { n >>= 1; ++r; } return r;
#endif
}

// ── Combinatorics: nCr mod prime ──────────────────────────────────────────────
// Precomputes factorials and inverse factorials for repeated nCr queries.
/// @brief Precomputes factorials for efficient combinatorial calculations modulo a prime.
/// @note Useful for repeated nCr and nPr queries under modulo.
struct Combinatorics {
    Array<u64> fact, inv_fact;
    u64 mod;

    /// @brief Constructs a Combinatorics instance for the given size and modulus.
    /// @param n Maximum n for nCr(n, r).
    /// @param mod Prime modulus.
    Combinatorics(usize n, u64 mod) : fact(n+1), inv_fact(n+1), mod(mod) {
        fact[0] = 1;
        for (usize i = 1; i <= n; ++i) fact[i] = fact[i-1] * i % mod;
        inv_fact[n] = mod_inverse(fact[n], mod);
        for (usize i = n; i-- > 0;) inv_fact[i] = inv_fact[i+1] * (i+1) % mod;
    }

    /// @brief Computes binomial coefficient nCr modulo mod.
    /// @param n Total items.
    /// @param r Items to choose.
    /// @return nCr % mod.
    [[nodiscard]] u64 nCr(usize n, usize r) const noexcept {
        if (r > n) return 0;
        return fact[n] * inv_fact[r] % mod * inv_fact[n-r] % mod;
    }

    /// @brief Computes permutation nPr modulo mod.
    /// @param n Total items.
    /// @param r Items to arrange.
    /// @return nPr % mod.
    [[nodiscard]] u64 nPr(usize n, usize r) const noexcept {
        if (r > n) return 0;
        return fact[n] * inv_fact[n-r] % mod;
    }
};

// ── Matrix<T, R, C> ────────────────────────────────────────────────────────────
// Fixed-size compile-time matrix.
/// @brief Fixed-size matrix with compile-time dimensions.
/// @tparam T Element type.
/// @tparam R Number of rows.
/// @tparam C Number of columns.
template<typename T, usize R, usize C>
struct Matrix {
    T data[R][C];

    /// @brief Default constructor. Initializes all elements to T{}.
    constexpr Matrix() noexcept {
        for (usize i = 0; i < R; ++i) for (usize j = 0; j < C; ++j) data[i][j] = T{};
    }

    /// @brief Accesses element at row r, column c.
    /// @param r Row index.
    /// @param c Column index.
    /// @return Reference to element at (r, c).
    T&       at(usize r, usize c)       noexcept { return data[r][c]; }
    /// @brief Accesses element at row r, column c (const).
    /// @param r Row index.
    /// @param c Column index.
    /// @return Const reference to element at (r, c).
    const T& at(usize r, usize c) const noexcept { return data[r][c]; }

    /// @brief Creates an identity matrix.
    /// @return Identity matrix.
    /// @pre R == C (square matrix).
    static Matrix identity() noexcept {
        static_assert(R == C, "Identity only for square matrices");
        Matrix m;
        for (usize i = 0; i < R; ++i) m.data[i][i] = T(1);
        return m;
    }

    /// @brief Matrix addition.
    /// @param o Other matrix.
    /// @return Sum of this and o.
    Matrix operator+(const Matrix& o) const noexcept {
        Matrix r;
        for (usize i = 0; i < R; ++i) for (usize j = 0; j < C; ++j)
            r.data[i][j] = data[i][j] + o.data[i][j];
        return r;
    }

    /// @brief Matrix multiplication.
    /// @tparam K Columns of the result matrix.
    /// @param o Other matrix.
    /// @return Product of this and o.
    template<usize K>
    Matrix<T, R, K> operator*(const Matrix<T, C, K>& o) const noexcept {
        Matrix<T, R, K> r;
        for (usize i = 0; i < R; ++i)
            for (usize k = 0; k < C; ++k)
                for (usize j = 0; j < K; ++j)
                    r.data[i][j] += data[i][k] * o.data[k][j];
        return r;
    }

    /// @brief Matrix equality comparison.
    /// @param o Other matrix.
    /// @return true if all elements are equal.
    bool operator==(const Matrix& o) const noexcept {
        for (usize i = 0; i < R; ++i) for (usize j = 0; j < C; ++j)
            if (data[i][j] != o.data[i][j]) return false;
        return true;
    }
};

// Matrix fast exponentiation: M^p in O(n³ log p)
/// @brief Computes matrix raised to a power using binary exponentiation.
/// @tparam T Element type.
/// @tparam N Matrix dimension.
/// @param m Base matrix.
/// @param p Exponent.
/// @return m^p.
/// @complexity O(N³ log p).
template<typename T, usize N>
Matrix<T,N,N> mat_pow(Matrix<T,N,N> m, u64 p) noexcept {
    Matrix<T,N,N> result = Matrix<T,N,N>::identity();
    while (p) {
        if (p & 1) result = result * m;
        m = m * m;
        p >>= 1;
    }
    return result;
}

// ── Number theory: Euler's totient ────────────────────────────────────────────
/// @brief Computes Euler's totient function φ(n).
/// @param n Positive integer.
/// @return Number of integers up to n that are coprime with n.
/// @complexity O(sqrt(n)).
[[nodiscard]] inline u64 euler_totient(u64 n) noexcept {
    u64 result = n;
    for (u64 p = 2; p * p <= n; ++p) {
        if (n % p == 0) {
            while (n % p == 0) n /= p;
            result -= result / p;
        }
    }
    if (n > 1) result -= result / n;
    return result;
}

// ── Prefix sums ───────────────────────────────────────────────────────────────
/// @brief Computes prefix sums of an array.
/// @tparam T Element type.
/// @param arr Input array.
/// @param n Array size.
/// @return Array of prefix sums, where ps[i+1] = sum of first i+1 elements.
template<typename T>
Array<T> prefix_sum(const T* arr, usize n) {
    Array<T> ps(n + 1);
    ps[0] = T{};
    for (usize i = 0; i < n; ++i) ps[i+1] = ps[i] + arr[i];
    return ps;
}

// 2D prefix sum on a flat row-major array
/// @brief Computes 2D prefix sums for a matrix stored in row-major order.
/// @tparam T Element type.
/// @param arr Input matrix as flat array.
/// @param rows Number of rows.
/// @param cols Number of columns.
/// @return 2D prefix sum array of size (rows+1)×(cols+1).
template<typename T>
Array<T> prefix_sum_2d(const T* arr, usize rows, usize cols) {
    Array<T> ps((rows+1)*(cols+1), T{});
    auto idx = [&](usize r, usize c){ return r*(cols+1)+c; };
    for (usize i = 1; i <= rows; ++i)
        for (usize j = 1; j <= cols; ++j)
            ps[idx(i,j)] = arr[(i-1)*cols+(j-1)] + ps[idx(i-1,j)] + ps[idx(i,j-1)] - ps[idx(i-1,j-1)];
    return ps;
}

} // namespace dsc
