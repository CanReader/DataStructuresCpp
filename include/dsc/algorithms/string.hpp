#pragma once

#include "../core/primitives.hpp"
#include "../core/memory.hpp"
#include "../containers/array.hpp"

// ── String algorithms ─────────────────────────────────────────────────────────
// All functions accept raw pointer + length for zero-copy operation.
// No string class dependency.

namespace dsc {

// ── KMP — Knuth-Morris-Pratt pattern search ───────────────────────────────────
// Find all occurrences of `pattern` in `text`. O(n + m).
// Avoids redundant comparisons by pre-computing the failure function.
//
// Returns array of starting positions (0-based) in text.
/// @brief Searches for all occurrences of pattern in text using Knuth-Morris-Pratt algorithm.
/// @param text Pointer to the text string.
/// @param n Length of the text.
/// @param pattern Pointer to the pattern string.
/// @param m Length of the pattern.
/// @return Array of starting positions (0-based) where pattern occurs in text.
/// @complexity O(n + m) where n is text length and m is pattern length.
inline Array<usize> kmp_search(const char* text,    usize n,
                                const char* pattern, usize m) {
    Array<usize> result;
    if (m == 0 || m > n) return result;

    // Build failure function (partial match table)
    Array<usize> fail(m, 0u);
    for (usize i = 1, j = 0; i < m;) {
        if (pattern[i] == pattern[j]) { fail[i++] = ++j; }
        else if (j)                   { j = fail[j - 1]; }
        else                          { fail[i++] = 0;   }
    }

    // Search
    for (usize i = 0, j = 0; i < n;) {
        if (text[i] == pattern[j]) {
            ++i; ++j;
            if (j == m) { result.push_back(i - m); j = fail[j - 1]; }
        } else if (j) {
            j = fail[j - 1];
        } else {
            ++i;
        }
    }
    return result;
}

// ── Rabin-Karp rolling hash search ────────────────────────────────────────────
// Find all occurrences of `pattern` in `text`. O(n + m) average, O(nm) worst.
// Multiple patterns can be searched in one pass (not shown here).
/// @brief Searches for all occurrences of pattern in text using Rabin-Karp algorithm.
/// @param text Pointer to the text string.
/// @param n Length of the text.
/// @param pattern Pointer to the pattern string.
/// @param m Length of the pattern.
/// @return Array of starting positions (0-based) where pattern occurs in text.
/// @complexity O(n + m) average, O(n * m) worst-case where n is text length and m is pattern length.
/// @note Uses rolling hash to avoid recomputing hash for each window.
inline Array<usize> rabin_karp_search(const char* text,    usize n,
                                       const char* pattern, usize m) {
    Array<usize> result;
    if (m == 0 || m > n) return result;

    constexpr u64 BASE = 31;
    constexpr u64 MOD  = 1000000007ULL;

    // Compute BASE^(m-1) % MOD
    u64 pow_m = 1;
    for (usize i = 0; i + 1 < m; ++i) pow_m = pow_m * BASE % MOD;

    // Hash of pattern and first window
    u64 ph = 0, wh = 0;
    for (usize i = 0; i < m; ++i) {
        ph = (ph * BASE + static_cast<u64>(pattern[i])) % MOD;
        wh = (wh * BASE + static_cast<u64>(text[i]))    % MOD;
    }

    // Roll the window
    for (usize i = 0; i + m <= n; ++i) {
        if (ph == wh) {
            // Hash match — verify character by character (avoid false positives)
            bool ok = true;
            for (usize j = 0; j < m; ++j) {
                if (text[i + j] != pattern[j]) { ok = false; break; }
            }
            if (ok) result.push_back(i);
        }
        if (i + m < n) {
            wh = (wh - static_cast<u64>(text[i]) * pow_m % MOD + MOD) % MOD;
            wh = (wh * BASE + static_cast<u64>(text[i + m])) % MOD;
        }
    }
    return result;
}

// ── Z-algorithm ───────────────────────────────────────────────────────────────
// Computes Z-array: Z[i] = length of the longest substring starting at text[i]
// that is also a prefix of text. O(n).
// Useful for pattern matching: concatenate pattern + '$' + text, compute Z-array.
/// @brief Computes the Z-array for the string using Z-algorithm.
/// @param s Pointer to the string.
/// @param n Length of the string.
/// @return Array where z[i] is the length of the longest substring starting at s[i] that matches a prefix of s.
/// @complexity O(n) where n is the string length.
/// @note z[0] is set to n by convention.
inline Array<usize> z_function(const char* s, usize n) {
    Array<usize> z(n, 0u);
    z[0] = n;
    usize l = 0, r = 0;
    for (usize i = 1; i < n; ++i) {
        if (i < r) z[i] = z[i - l] < r - i ? z[i - l] : r - i;
        while (i + z[i] < n && s[z[i]] == s[i + z[i]]) ++z[i];
        if (i + z[i] > r) { l = i; r = i + z[i]; }
    }
    return z;
}

// Z-based pattern search — finds all occurrences of pattern in text
/// @brief Searches for all occurrences of pattern in text using Z-algorithm.
/// @param text Pointer to the text string.
/// @param n Length of the text.
/// @param pattern Pointer to the pattern string.
/// @param m Length of the pattern.
/// @return Array of starting positions (0-based) where pattern occurs in text.
/// @complexity O(n + m) where n is text length and m is pattern length.
inline Array<usize> z_search(const char* text,    usize n,
                               const char* pattern, usize m) {
    // Build concat = pattern + '$' + text
    usize total = m + 1 + n;
    char* concat = static_cast<char*>(::operator new(total));
    mem::copy(concat,         pattern, m);
    concat[m] = '$';
    mem::copy(concat + m + 1, text,    n);

    Array<usize> z = z_function(concat, total);
    ::operator delete(concat);

    Array<usize> result;
    for (usize i = m + 1; i < total; ++i)
        if (z[i] >= m) result.push_back(i - m - 1);
    return result;
}

// ── Boyer-Moore-Horspool ───────────────────────────────────────────────────────
// Practical fastest string search for long patterns. O(n/m) best case, O(nm) worst.
// Uses the "bad character" skip table — 256-entry shift array.
/// @brief Searches for all occurrences of pattern in text using Boyer-Moore-Horspool algorithm.
/// @param text Pointer to the text string.
/// @param n Length of the text.
/// @param pattern Pointer to the pattern string.
/// @param m Length of the pattern.
/// @return Array of starting positions (0-based) where pattern occurs in text.
/// @complexity O(n/m) best-case, O(n * m) worst-case where n is text length and m is pattern length.
/// @note Most efficient for long patterns and large alphabets.
inline Array<usize> bmh_search(const char* text,    usize n,
                                const char* pattern, usize m) {
    Array<usize> result;
    if (m == 0 || m > n) return result;

    // Bad character table: skip[c] = m - 1 - last occurrence of c in pattern[0..m-2]
    usize skip[256];
    for (usize i = 0; i < 256; ++i) skip[i] = m;
    for (usize i = 0; i + 1 < m; ++i) skip[static_cast<u8>(pattern[i])] = m - 1 - i;

    usize i = 0;
    while (i + m <= n) {
        usize j = m - 1;
        while (j < m && text[i + j] == pattern[j]) {
            if (j == 0) { result.push_back(i); break; }
            --j;
        }
        i += skip[static_cast<u8>(text[i + m - 1])];
    }
    return result;
}

// ── Longest Common Subsequence (LCS) ─────────────────────────────────────────
// Returns the length of the LCS of s1[0..n1) and s2[0..n2). O(n1 * n2).
/// @brief Computes the length of the longest common subsequence between two strings.
/// @param s1 Pointer to the first string.
/// @param n1 Length of the first string.
/// @param s2 Pointer to the second string.
/// @param n2 Length of the second string.
/// @return Length of the longest common subsequence.
/// @complexity O(n1 * n2) where n1 and n2 are the string lengths.
/// @note Space-optimized to O(min(n1, n2)) using rolling arrays.
inline usize lcs_length(const char* s1, usize n1,
                         const char* s2, usize n2) {
    // Space-optimised: O(min(n1,n2)) using two rolling rows
    if (n1 < n2) {
        usize tmp = n1; n1 = n2; n2 = tmp;
        const char* t = s1; s1 = s2; s2 = t;
    }
    Array<usize> prev(n2 + 1, 0u), curr(n2 + 1, 0u);
    for (usize i = 1; i <= n1; ++i) {
        for (usize j = 1; j <= n2; ++j) {
            if (s1[i-1] == s2[j-1]) curr[j] = prev[j-1] + 1;
            else curr[j] = curr[j-1] > prev[j] ? curr[j-1] : prev[j];
        }
        for (usize j = 0; j <= n2; ++j) { prev[j] = curr[j]; curr[j] = 0; }
    }
    return prev[n2];
}

// ── Edit distance (Levenshtein) ───────────────────────────────────────────────
// Minimum insert/delete/substitute ops to convert s1 into s2. O(n1 * n2).
/// @brief Computes the edit distance (Levenshtein distance) between two strings.
/// @param s1 Pointer to the first string.
/// @param n1 Length of the first string.
/// @param s2 Pointer to the second string.
/// @param n2 Length of the second string.
/// @return Minimum number of operations to transform s1 into s2.
/// @complexity O(n1 * n2) where n1 and n2 are the string lengths.
/// @note Operations are insert, delete, and substitute.
inline usize edit_distance(const char* s1, usize n1,
                             const char* s2, usize n2) {
    Array<usize> prev(n2 + 1), curr(n2 + 1, 0u);
    for (usize j = 0; j <= n2; ++j) prev[j] = j;
    for (usize i = 1; i <= n1; ++i) {
        curr[0] = i;
        for (usize j = 1; j <= n2; ++j) {
            if (s1[i-1] == s2[j-1]) curr[j] = prev[j-1];
            else {
                usize mn = prev[j-1] < prev[j] ? prev[j-1] : prev[j];
                mn = mn < curr[j-1] ? mn : curr[j-1];
                curr[j] = 1 + mn;
            }
        }
        for (usize j = 0; j <= n2; ++j) { prev[j] = curr[j]; curr[j] = 0; }
    }
    return prev[n2];
}

// ── Manacher's algorithm — Longest Palindromic Substring ─────────────────────
// Computes radius of the longest palindrome centred at each position. O(n).
// Works on the transformed string '|a|b|a|' to handle even-length palindromes.
/// @brief Computes palindrome radii for each position using Manacher's algorithm.
/// @param s Pointer to the string.
/// @param n Length of the string.
/// @return Array where p[i]/2 is the radius of the longest palindrome centered at position i in the transformed string.
/// @complexity O(n) where n is the string length.
/// @note Handles both odd and even length palindromes.
inline Array<usize> manacher(const char* s, usize n) {
    // Transform to '|a|b|a|b|' form
    usize tn = 2 * n + 1;
    char* t = static_cast<char*>(::operator new(tn));
    for (usize i = 0; i < n; ++i) { t[2*i] = '|'; t[2*i+1] = s[i]; }
    t[2*n] = '|';

    Array<usize> p(tn, 0u);
    usize c = 0, r = 0;
    for (usize i = 0; i < tn; ++i) {
        usize mirror = 2*c - i;
        if (i < r) p[i] = (r - i) < p[mirror] ? (r - i) : p[mirror];
        while (i + p[i] + 1 < tn && i >= p[i] + 1 && t[i + p[i] + 1] == t[i - p[i] - 1])
            ++p[i];
        if (i + p[i] > r) { c = i; r = i + p[i]; }
    }
    ::operator delete(t);
    return p;  // p[i]/2 = radius of palindrome centred at i in original string
}

// ── Suffix Array (SA-IS light) ────────────────────────────────────────────────
// O(n log n) suffix array construction via radix-sort-based approach.
// Returns suffix array SA where SA[i] = start index of i-th lexicographic suffix.
/// @brief Constructs a suffix array for the string using a prefix-doubling approach.
/// @param s Pointer to the string.
/// @param n Length of the string.
/// @return Suffix array where sa[i] is the starting index of the i-th lexicographically smallest suffix.
/// @complexity O(n log n) where n is the string length.
/// @note Useful for string matching and other string algorithms.
inline Array<usize> suffix_array(const char* s, usize n) {
    Array<usize> sa(n);
    for (usize i = 0; i < n; ++i) sa[i] = i;

    // Initial sort by first character
    // Use counting sort for stability
    usize cnt[256] = {};
    for (usize i = 0; i < n; ++i) ++cnt[static_cast<u8>(s[i])];
    usize pos[256] = {};
    usize sum = 0;
    for (usize i = 0; i < 256; ++i) { pos[i] = sum; sum += cnt[i]; }
    Array<usize> tmp(n);
    for (usize i = 0; i < n; ++i) tmp[pos[static_cast<u8>(s[i])]++] = i;
    for (usize i = 0; i < n; ++i) sa[i] = tmp[i];

    // Prefix-doubling (Manber-Myers style)
    Array<usize> rank_(n), rank2(n);
    for (usize i = 0; i < n; ++i) rank_[sa[i]] = i > 0 && s[sa[i]] == s[sa[i-1]] ? rank_[sa[i-1]] : i;

    for (usize gap = 1; gap < n; gap *= 2) {
        auto cmp = [&](usize a, usize b) {
            if (rank_[a] != rank_[b]) return rank_[a] < rank_[b];
            usize ra = a + gap < n ? rank_[a + gap] : usize(-1);
            usize rb = b + gap < n ? rank_[b + gap] : usize(-1);
            return ra < rb;
        };
        introsort(sa.data(), sa.data() + n, cmp);

        rank2[sa[0]] = 0;
        for (usize i = 1; i < n; ++i)
            rank2[sa[i]] = rank2[sa[i-1]] + (cmp(sa[i-1], sa[i]) ? 1 : 0);
        for (usize i = 0; i < n; ++i) rank_[i] = rank2[i];
        if (rank_[sa[n-1]] == n-1) break;
    }
    return sa;
}

} // namespace dsc
