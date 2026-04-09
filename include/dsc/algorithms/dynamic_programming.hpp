#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../containers/array.hpp"

// ── Dynamic Programming algorithms ────────────────────────────────────────────
// Essential DP patterns without STL dependency.

namespace dsc {

// ── Longest Increasing Subsequence (full reconstruction) ────────────────────
/// @brief Computes the longest increasing subsequence and returns its length.
/// @tparam T Element type (must support comparison).
/// @param arr Input array.
/// @param n Array size.
/// @return Length of the LIS.
/// @complexity O(n²) time, O(n) space.
template<typename T>
[[nodiscard]] usize lis_length(const T* arr, usize n) noexcept {
    if (n == 0) return 0;
    
    Array<usize> dp(n, 1);
    usize result = 1;
    
    for (usize i = 1; i < n; ++i) {
        for (usize j = 0; j < i; ++j) {
            if (arr[j] < arr[i]) {
                dp[i] = traits::max(dp[i], dp[j] + 1);
            }
        }
        result = traits::max(result, dp[i]);
    }
    return result;
}

// ── Longest Common Subsequence (LCS) ──────────────────────────────────────────
/// @brief Computes the length of the longest common subsequence.
/// @tparam T Element type (must support equality comparison).
/// @param a First sequence.
/// @param m Length of first sequence.
/// @param b Second sequence.
/// @param n Length of second sequence.
/// @return Length of the LCS.
/// @complexity O(m*n) time and space.
template<typename T>
[[nodiscard]] usize lcs_length(const T* a, usize m, const T* b, usize n) noexcept {
    if (m == 0 || n == 0) return 0;
    
    Array<Array<usize>> dp(m + 1);
    for (usize i = 0; i <= m; ++i) {
        dp[i] = Array<usize>(n + 1, 0);
    }
    
    for (usize i = 1; i <= m; ++i) {
        for (usize j = 1; j <= n; ++j) {
            if (a[i - 1] == b[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = traits::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }
    return dp[m][n];
}

// ── 0/1 Knapsack (unbounded variant) ──────────────────────────────────────────
/// @brief Solves the 0/1 knapsack problem.
/// @param weights Array of item weights.
/// @param values Array of item values.
/// @param n Number of items.
/// @param capacity Maximum knapsack capacity.
/// @return Maximum value achievable.
/// @complexity O(n * capacity) time, O(capacity) space.
[[nodiscard]] u64 knapsack_01(const u64* weights, const u64* values, usize n, u64 capacity) noexcept {
    Array<u64> dp(capacity + 1, 0);
    
    for (usize i = 0; i < n; ++i) {
        for (u64 w = capacity; w >= weights[i]; --w) {
            dp[w] = traits::max(dp[w], dp[w - weights[i]] + values[i]);
        }
    }
    return dp[capacity];
}

// ── Unbounded Knapsack ────────────────────────────────────────────────────────
/// @brief Solves the unbounded knapsack problem (items can be reused).
/// @param weights Array of item weights.
/// @param values Array of item values.
/// @param n Number of items.
/// @param capacity Maximum knapsack capacity.
/// @return Maximum value achievable.
/// @complexity O(n * capacity) time, O(capacity) space.
[[nodiscard]] u64 knapsack_unbounded(const u64* weights, const u64* values, usize n, u64 capacity) noexcept {
    Array<u64> dp(capacity + 1, 0);
    
    for (u64 w = 1; w <= capacity; ++w) {
        for (usize i = 0; i < n; ++i) {
            if (weights[i] <= w) {
                dp[w] = traits::max(dp[w], dp[w - weights[i]] + values[i]);
            }
        }
    }
    return dp[capacity];
}

// ── Coin Change (minimum coins) ───────────────────────────────────────────────
/// @brief Computes the minimum number of coins needed to make a target amount.
/// @param coins Array of coin denominations.
/// @param n Number of distinct coins.
/// @param amount Target amount.
/// @return Minimum coins needed, or -1 if impossible.
/// @complexity O(amount * n) time, O(amount) space.
[[nodiscard]] isize coin_change_min(const u64* coins, usize n, u64 amount) noexcept {
    if (amount == 0) return 0;
    
    Array<u64> dp(amount + 1, INF<u64>);
    dp[0] = 0;
    
    for (u64 i = 1; i <= amount; ++i) {
        for (usize j = 0; j < n; ++j) {
            if (coins[j] <= i && dp[i - coins[j]] != INF<u64>) {
                dp[i] = traits::min(dp[i], dp[i - coins[j]] + 1);
            }
        }
    }
    
    return (dp[amount] == INF<u64>) ? -1 : static_cast<isize>(dp[amount]);
}

// ── Edit Distance (Levenshtein Distance) ──────────────────────────────────────
/// @brief Computes the minimum edit distance between two strings.
/// @param s1 First string.
/// @param m Length of first string.
/// @param s2 Second string.
/// @param n Length of second string.
/// @return Minimum number of edits (insert, delete, replace).
/// @complexity O(m*n) time and space.
[[nodiscard]] usize edit_distance(const char* s1, usize m, const char* s2, usize n) noexcept {
    Array<Array<usize>> dp(m + 1);
    for (usize i = 0; i <= m; ++i) {
        dp[i] = Array<usize>(n + 1);
    }
    
    for (usize i = 0; i <= m; ++i) dp[i][0] = i;
    for (usize j = 0; j <= n; ++j) dp[0][j] = j;
    
    for (usize i = 1; i <= m; ++i) {
        for (usize j = 1; j <= n; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                usize ins = dp[i][j - 1];
                usize del = dp[i - 1][j];
                usize rep = dp[i - 1][j - 1];
                dp[i][j] = 1 + traits::min({ins, del, rep});
            }
        }
    }
    return dp[m][n];
}

// ── Rod Cutting Problem ───────────────────────────────────────────────────────
/// @brief Finds the maximum revenue from cutting a rod of given length.
/// @param prices Price of rod pieces[1], [2], ..., [n].
/// @param n Length of rod.
/// @return Maximum revenue.
/// @complexity O(n²) time, O(n) space.
[[nodiscard]] u64 rod_cutting(const u64* prices, usize n) noexcept {
    Array<u64> dp(n + 1, 0);
    
    for (usize i = 1; i <= n; ++i) {
        u64 maxRev = 0;
        for (usize j = 1; j <= i; ++j) {
            maxRev = traits::max(maxRev, prices[j - 1] + dp[i - j]);
        }
        dp[i] = maxRev;
    }
    return dp[n];
}

// ── Matrix Chain Multiplication (optimal ordering) ───────────────────────────
/// @brief Finds optimal order to multiply matrices to minimize scalar multiplications.
/// @param dims Array of matrix dimensions p[0]×p[1], p[1]×p[2], ..., p[n-1]×p[n].
/// @param n Number of matrices (dims.size() = n+1).
/// @return Minimum number of scalar multiplications.
/// @complexity O(n³) time, O(n²) space.
[[nodiscard]] u64 matrix_chain_order(const usize* dims, usize n) noexcept {
    if (n <= 1) return 0;
    
    Array<Array<u64>> dp(n);
    for (usize i = 0; i < n; ++i) {
        dp[i] = Array<u64>(n, 0);
    }
    
    for (usize len = 2; len <= n; ++len) {
        for (usize i = 0; i < n - len + 1; ++i) {
            usize j = i + len - 1;
            dp[i][j] = INF<u64>;
            for (usize k = i; k < j; ++k) {
                u64 cost = dp[i][k] + dp[k + 1][j] + dims[i] * dims[k + 1] * dims[j + 1];
                dp[i][j] = traits::min(dp[i][j], cost);
            }
        }
    }
    return dp[0][n - 1];
}

} // namespace dsc
