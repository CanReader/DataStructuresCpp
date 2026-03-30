# dsc — Data Structures C++

> A zero-dependency, high-performance C++20 data structures and algorithms library.
> **No standard library. No Boost. No libc. Everything from scratch.**

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Header-only](https://img.shields.io/badge/header--only-yes-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)
![Zero dependencies](https://img.shields.io/badge/dependencies-none-orange)

---

## What makes dsc different?

| Feature | dsc | std | Boost |
|---|---|---|---|
| Standard library dependency | **None** | Required | Required |
| Dynamic array growth factor | **1.5×** (less waste) | 2× | 2× |
| Hash map strategy | **Robin Hood open-addressing** | Chaining | varies |
| BST implementation | **AVL** (faster lookups) | Red-Black | Red-Black |
| Priority queue arity | **4-ary** (fewer cache misses) | Binary | Binary |
| Hash function | **FNV-1a + Fibonacci mixing** | impl-defined | varies |
| Memory ops | **`__builtin_memcpy/memset`** (SIMD auto-vectorised) | libc | libc |
| Exceptions | **None** — `Optional<T>` everywhere | Thrown | Mixed |
| Extra containers | Trie, SkipList, SegTree, Fenwick, SparseSet, BitSet, UnionFind | ❌ | Partial |
| Extra algorithms | Graph, String, Math, Sequence (30+ algorithms) | Minimal | Partial |

---

## Quick Start

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(dsc GIT_REPOSITORY https://github.com/yourname/DataStructuresCPP.git GIT_TAG main)
FetchContent_MakeAvailable(dsc)
target_link_libraries(your_target PRIVATE dsc::dsc)
```

### Single include

```cpp
#include <dsc/dsc.hpp>
using namespace dsc;
```

---

## Containers

### `Array<T>` — Dynamic Array
1.5× growth, memcpy fast-path for trivial types, full random-access iterator.
```cpp
Array<int> a;
a.push_back(1); a.emplace_back(2);
a.insert(a.begin(), 0);
a.erase(a.begin());
a.reserve(100); a.shrink_to_fit();
bool found = a.contains(2);
isize idx  = a.index_of(2);
for (int x : a) { /* ... */ }
```

### `LinkedList<T>` — Doubly Linked List
Sentinel-node design, O(1) insert/erase with iterator.
```cpp
LinkedList<int> l;
l.push_front(1); l.push_back(2);
auto it = l.find(1); l.erase(it);
l.contains(2);
```

### `Stack<T>` — LIFO Stack
```cpp
Stack<int> s;
s.push(1); int top = s.pop_value(); s.top();
```

### `Queue<T>` — Circular-Buffer FIFO
O(1) enqueue/dequeue, no element shifting.
```cpp
Queue<int> q;
q.enqueue(1); int v = q.dequeue();
q.front(); q.back();
```

### `Deque<T, BLOCK>` — Double-Ended Queue
Block-segmented, O(1) push/pop at both ends, O(1) random access.
```cpp
Deque<int> d;
d.push_front(0); d.push_back(1);
d.pop_front(); int v = d[2];
```

### `HashMap<K, V>` — Robin Hood Hash Map
Backward-shift deletion (no tombstones), 87.5% max load factor.
```cpp
HashMap<int, int> m;
m.insert(1, 10); m.insert_or_assign(1, 20);
m[2] = 30;
auto opt = m.find(1);  // Optional<int*>
m.erase(1); m.contains(2);
for (auto [k, v] : m) { /* ... */ }
```

### `HashSet<K>` — Robin Hood Hash Set
```cpp
HashSet<int> s;
s.insert(1); s.erase(1); s.contains(2);
```

### `TreeMap<K, V>` — AVL Tree Map
Sorted order, stricter balance than Red-Black → faster lookups.
```cpp
TreeMap<int, int> m;
m.insert(3, 30); m[4] = 40;
auto opt = m.find(3);   // Optional<int*>
m.min_key(); m.max_key();
for (auto [k, v] : m) { /* sorted */ }
```

### `TreeSet<K>` — AVL Tree Set
```cpp
TreeSet<int> s;
s.insert(5); s.erase(5); s.contains(3);
s.min_key(); s.max_key();
for (const int& k : s) { /* sorted */ }
```

### `PriorityQueue<T, Cmp>` — 4-ary Heap
```cpp
MaxHeap<int> h;  MinHeap<int> mh;
h.push(5); h.push(1); h.top();  // 5
h.pop(); h.pop_value();
int arr[] = {3,1,4}; MaxHeap<int> h2(arr, arr+3);  // O(n) build
```

### `Trie<CharT>` — Prefix Tree
```cpp
Trie<char> t;
t.insert("hello", 5); t.insert("world", 5);
t.contains("hello", 5);        // true
t.starts_with("hel", 3);       // true
t.count_with_prefix("hel", 3); // 1
t.erase("hello", 5);
```

### `BitSet<N>` / `DynBitSet` — Compact Bit Array
Uses `__builtin_popcountll` → POPCNT instruction automatically.
```cpp
BitSet<64> b;
b.set(3); b.reset(3); b.flip(7);
b.test(7); b.count(); b.any(); b.all(); b.none();
b.lowest_set();
BitSet<64> c = b & ~b;   // bitwise ops

DynBitSet db(1000);      // runtime size
db.set(42); db.count();
```

### `UnionFind` — Disjoint Set Union
Path compression + union by rank → O(α(n)) per operation.
```cpp
UnionFind uf(10);
uf.unite(0, 1); uf.unite(1, 2);
uf.connected(0, 2);       // true
uf.component_count();     // 8
uf.find(2);               // root of component
```

### `SegmentTree<T, Combine>` — Range Query Tree
Point update + range query in O(log n). Lazy version for range updates.
```cpp
int arr[] = {1,3,5,7,9};
// Range sum
SegmentTree<int> seg(arr, 5, 0);
seg.update(2, 10);         // arr[2] = 10
seg.query(1, 3);           // sum of [1..3]

// Range min
SegmentTree<int, decltype([](int a, int b){ return a<b?a:b; })>
    seg_min(arr, 5, 2147483647);
```

### `FenwickTree<T>` — Binary Indexed Tree
Most space-efficient prefix-sum structure. O(log n) update/query.
```cpp
int arr[] = {1,2,3,4,5};
FenwickTree<int> ft(arr, 5);
ft.update(2, 10);          // add 10 to index 2
ft.prefix_sum(4);          // sum [0..4]
ft.range_sum(1, 3);        // sum [1..3]
ft.lower_bound(10);        // smallest i where prefix_sum(i) >= 10

FenwickTree2D<int> ft2(n, m);  // 2D variant
ft2.update(r, c, v); ft2.range_sum(r1,c1,r2,c2);
```

### `SkipList<K, V>` — Probabilistic Sorted Map
O(log n) expected insert/erase/find. Lock-friendly, cache-sequential scan.
```cpp
SkipList<int, int> sl;
sl.insert(3, 30); sl.insert(1, 10);
auto opt = sl.find(3);     // Optional<int*>
sl.erase(1);
for (auto [k, v] : sl) { /* sorted */ }
```

### `SparseSet<T>` — O(1) Integer Set
Ultra-fast iteration and O(1) all operations for integers < universe.
```cpp
SparseSet<int> ss(1000);   // elements must be < 1000
ss.insert(42); ss.insert(7);
ss.contains(42);           // true
ss.erase(42);
ss.clear();                // O(1) — no init needed
for (int x : ss) { /* cache-sequential */ }
```

---

## Algorithms

### Sorting (`algorithms/sort.hpp`)
```cpp
int a[] = {5,3,8,1};
sort(a, a+4);                              // introsort (default)
introsort(a, a+4, [](int x,int y){return x>y;});  // descending
merge_sort(a, a+4);                        // stable
heap_sort(a, a+4);
counting_sort(a, a+4);                     // integers only
unsigned u[] = {42,5,17}; radix_sort(u, u+3);  // unsigned only
```

### Searching (`algorithms/search.hpp`)
```cpp
int s[] = {1,3,5,7,9};
int* p  = binary_search(s, s+5, 7);       // pointer or nullptr
bool ok = binary_contains(s, s+5, 5);
int* lo = lower_bound(s, s+5, 5);         // first >= 5
int* hi = upper_bound(s, s+5, 5);         // first > 5
int* q  = linear_search(s, s+5, 3);
int* r  = interpolation_search(s, s+5, 9); // O(log log n) avg
```

### Graph algorithms (`algorithms/graph.hpp`)
```cpp
// Graph as adjacency list
Array<Array<Edge>> g(n);
g[0].push_back({1}); g[1].push_back({2});

Array<usize> dist    = bfs(g, 0);                  // hop distances from src
Array<usize> order   = dfs(g, 0);                  // DFS finish order
Array<usize> topo    = topological_sort(g);         // Kahn's algorithm
Array<usize> comp    = connected_components(g);     // component IDs
bool         cycle   = has_cycle_directed(g);
bool         bip     = is_bipartite(g);

// Weighted graph
Array<Array<WeightedEdge<int>>> wg(n);
wg[0].push_back({1, 4}); wg[0].push_back({2, 1});

Array<int>   dijkstra_dist = dijkstra(wg, 0);
auto [d, par]  = dijkstra_parents(wg, 0);           // structured binding

// Bellman-Ford (handles negative edges)
Array<BFEdge<int>> edges = {{0,1,-1},{1,2,4}};
auto [bf_dist, neg_cycle] = bellman_ford(edges, n, 0);

// Floyd-Warshall (all-pairs)
Array<int> mat = floyd_warshall(n, edges);
int dist_u_v   = mat[u*n + v];

// MST
Array<UndirectedEdge<int>> ued = {{0,1,1},{1,2,3},{0,2,2}};
auto kruskal_mst = kruskal(n, ued);                 // O(E log E)
auto prim_mst    = prim(wg);                        // O((V+E) log V)
```

### String algorithms (`algorithms/string.hpp`)
```cpp
// Pattern search
auto pos1 = kmp_search("abcabc", 6, "abc", 3);    // {0, 3}
auto pos2 = rabin_karp_search(text, n, pat, m);
auto pos3 = z_search(text, n, pat, m);
auto pos4 = bmh_search(text, n, pat, m);           // fastest in practice

// Dynamic programming on strings
usize lcs = lcs_length("abcde", 5, "ace", 3);     // 3
usize ed  = edit_distance("kitten", 6, "sitting", 7); // 3

// Palindromes
auto radii = manacher("racecar", 7);               // palindrome radii

// Suffix array
auto sa = suffix_array("banana", 6);               // lexicographic order

// Z-function
auto z = z_function("aabxaa", 6);
```

### Math algorithms (`algorithms/math.hpp`)
```cpp
gcd(12, 8);           // 4
lcm(4, 6);            // 12
pow_int(2, 10);       // 1024
pow_mod(2, 10, 1009); // (2^10) % 1009
mod_inverse(3, 1e9+7); // modular inverse
is_prime(104729);     // true (Miller-Rabin)
isqrt(144);           // 12
ilog2(1024);          // 10
euler_totient(36);    // 12

auto primes = sieve_primes(100);           // {2,3,5,...,97}
auto big_p  = segmented_sieve(1e9, 1e9+1000); // large range

// Combinatorics mod prime
Combinatorics C(1000, 1000000007);
C.nCr(10, 3);   // 120
C.nPr(10, 3);   // 720

// Matrix exponentiation (Fibonacci in O(log n))
Matrix<u64, 2, 2> fib_mat;
fib_mat.at(0,0)=1; fib_mat.at(0,1)=1;
fib_mat.at(1,0)=1; fib_mat.at(1,1)=0;
auto result = mat_pow(fib_mat, 50);   // fib(50)

// Prefix sums
auto ps   = prefix_sum(arr, n);
auto ps2d = prefix_sum_2d(grid, rows, cols);
```

### Sequence algorithms (`algorithms/sequence.hpp`)
```cpp
// Kadane's max subarray
auto [sum, lo, hi] = max_subarray(arr, n);

// LIS — Longest Increasing Subsequence
usize len = lis_length(arr, n);           // O(n log n)

// Sliding window
auto win_max = sliding_window_max(arr, n, k);
auto win_min = sliding_window_min(arr, n, k);

// Permutations
next_permutation(arr, arr+n);
prev_permutation(arr, arr+n);

// Shuffle
shuffle(arr, arr+n, [](usize lim){ return rng() % lim; });

// Monotone stack
auto ng = next_greater(arr, n);           // next greater element indices
auto ph = prev_greater(arr, n);

// Classic problems
usize area = largest_rect_histogram(heights, n);
dutch_national_flag(arr, n, pivot);
rotate_left(arr, n, k);                   // three-reverse trick, O(1) space
usize nlen = remove_duplicates(arr, n);   // from sorted array

// Inversions
u64 inv_count = count_inversions(arr, n); // O(n log n)

// Dynamic programming
KnapsackItem items[] = {{2,3},{3,4},{4,5}};
u64 max_val = knapsack_01(items, 3, 5);

u64 coins[] = {1, 5, 10};
usize min_c = coin_change(coins, 3, 27);

// Boyer-Moore majority
int maj = majority_candidate(arr, n);
```

---

## Core Utilities

### `Optional<T>` — Nullable value (no exceptions)
```cpp
Optional<int> opt = some(42);
if (opt) { int v = *opt; }
opt.value_or(0); opt.reset();
```

### `Hash<T>` — FNV-1a + Fibonacci mixing
Built-in for all integer types, float, double, pointers. Extend:
```cpp
template<> struct dsc::Hash<MyType> {
    dsc::u64 operator()(const MyType& v) const noexcept {
        return dsc::detail::fnv1a(&v, sizeof(v));
    }
};
```

### `PoolAllocator<T, Capacity>` — O(1) node allocator
```cpp
PoolAllocator<Node, 1024> pool;
Node* n = pool.allocate();
pool.construct(n, args...);
pool.destroy(n); pool.deallocate(n);
```

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/dsc_example
```

---

## Design Principles

1. **Zero dependencies** — not a single `#include <std...>`. Type traits use `__is_trivially_copyable` etc.; memory ops use `__builtin_memcpy`; allocation via `operator new` (C++ ABI, not STL).
2. **No exceptions** — every fallible operation returns `Optional<T>`.
3. **Move semantics everywhere** — containers never copy when they can move.
4. **Trivial-type fast paths** — compiler auto-vectorises `memcpy`/`memmove` into SIMD.
5. **Algorithmic excellence** — introsort O(n log n) worst-case; AVL strict balance; Robin Hood bounded probes; 4-ary heap half depth; Fenwick O(log n) range queries.

---

## File Map

```
include/dsc/
├── core/
│   ├── primitives.hpp   type aliases (i8–u64, usize, isize)
│   ├── memory.hpp       __builtin_memcpy/memmove/memset wrappers
│   ├── traits.hpp       is_trivially_copyable, move, forward, swap
│   ├── optional.hpp     Optional<T>
│   ├── iterator.hpp     CRTP RandomAccess/Bidirectional iterator bases
│   ├── allocator.hpp    Allocator<T>, PoolAllocator<T,N>
│   └── hash.hpp         Hash<T> FNV-1a + Fibonacci mixing
├── containers/
│   ├── array.hpp         dynamic array
│   ├── linked_list.hpp   doubly linked list
│   ├── stack.hpp         LIFO stack
│   ├── queue.hpp         circular-buffer FIFO
│   ├── deque.hpp         block-segmented deque
│   ├── hash_map.hpp      Robin Hood map
│   ├── hash_set.hpp      Robin Hood set
│   ├── tree_map.hpp      AVL ordered map
│   ├── tree_set.hpp      AVL ordered set
│   ├── priority_queue.hpp 4-ary heap
│   ├── trie.hpp          prefix tree
│   ├── bitset.hpp        fixed + dynamic bitset
│   ├── union_find.hpp    DSU with path compression
│   ├── segment_tree.hpp  point-update range-query + lazy
│   ├── fenwick_tree.hpp  BIT + 2D BIT
│   ├── skip_list.hpp     probabilistic sorted map
│   └── sparse_set.hpp    O(1) integer set
└── algorithms/
    ├── sort.hpp     introsort, merge, heap, counting, radix sort
    ├── search.hpp   binary, interpolation, bounds
    ├── graph.hpp    BFS, DFS, Dijkstra, Bellman-Ford, Floyd-Warshall,
    │               Kruskal, Prim, topo-sort, SCC, bipartite
    ├── string.hpp   KMP, Rabin-Karp, Z-algo, Boyer-Moore, LCS,
    │               edit distance, Manacher, suffix array
    ├── math.hpp     GCD, LCM, fast-pow, mod-inverse, Miller-Rabin,
    │               sieve, matrix exponentiation, combinatorics, nCr
    └── sequence.hpp Kadane, LIS, sliding-window, knapsack, coin-change,
                    inversions, permutations, Dutch flag, monotone stack
```

---

## License

MIT © 2025
