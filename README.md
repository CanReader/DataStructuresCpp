# dsc — Data Structures C++

> A zero-dependency, high-performance C++20 data structures and algorithms library.
> **No standard library. No Boost. Everything from scratch.**

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Header-only](https://img.shields.io/badge/header--only-yes-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)
![Zero dependencies](https://img.shields.io/badge/dependencies-none-orange)

---

## What makes dsc different?

| Feature | dsc | std | Boost |
|---|---|---|---|
| Standard library dependency | **None** | Required | Required |
| Growth factor (dynamic array) | **1.5×** (less memory waste) | 2× | 2× |
| Hash map strategy | **Robin Hood open-addressing** | Chaining | varies |
| BST implementation | **AVL** (stricter balance → faster lookups) | Red-Black | Red-Black |
| Priority queue arity | **4-ary** (fewer cache misses) | Binary | Binary |
| Hash function | **FNV-1a + Fibonacci mixing** | Implementation-defined | varies |
| Sorting | **Introsort + counting + radix** | Implementation-defined | varies |
| Custom allocator | **Pool + heap** | Stateless only | varies |
| Exceptions | **None — Optional\<T\> everywhere** | Thrown by default | Mixed |

---

## Quick Start

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    dsc
    GIT_REPOSITORY https://github.com/yourname/DataStructuresCPP.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(dsc)

target_link_libraries(your_target PRIVATE dsc::dsc)
```

### Single include

```cpp
#include <dsc/dsc.hpp>

int main() {
    dsc::Array<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    dsc::sort(arr.data(), arr.data() + arr.size());

    dsc::HashMap<int, int> map;
    map.insert(42, 100);

    auto found = map.find(42);
    if (found) { /* *found is int* */ }
}
```

---

## Containers

### `dsc::Array<T, Alloc>` — Dynamic Array

Cache-friendly contiguous buffer with **1.5× growth factor**.

```cpp
dsc::Array<int> arr;
arr.push_back(1);               // O(1) amortized
arr.emplace_back(2);            // construct in-place, no copy
arr.insert(arr.begin(), 0);     // O(n) — shifts elements
arr.erase(arr.begin());         // O(n) — shifts elements
arr.reserve(100);               // pre-allocate
arr.shrink_to_fit();            // release unused memory

// Range-based for
for (int x : arr) { /* ... */ }

// Random access
int v = arr[0];
arr[0] = 99;

arr.contains(99);               // O(n) linear scan
arr.index_of(99);               // returns isize, -1 if absent
```

**Complexity:** `push_back` O(1) amort · `[]` O(1) · `insert`/`erase` O(n) · `reserve` O(n)

---

### `dsc::LinkedList<T>` — Doubly Linked List

Sentinel-node design — no null checks in hot paths.

```cpp
dsc::LinkedList<int> lst;
lst.push_back(1);
lst.push_front(0);
lst.emplace_back(2);

lst.pop_front();
lst.pop_back();

auto it = lst.find(1);
if (it != lst.end()) lst.erase(it);

lst.contains(42);   // O(n)
```

**Complexity:** `push`/`pop` O(1) · `find`/`contains` O(n) · `insert`/`erase` with iterator O(1)

---

### `dsc::Stack<T>` — LIFO Stack

Backed by `Array<T>` for cache-friendly storage.

```cpp
dsc::Stack<int> s;
s.push(1);
s.push(2);
int top = s.pop_value();   // removes and returns
int& ref = s.top();        // peek without removing
```

**Complexity:** `push`/`pop`/`top` O(1)

---

### `dsc::Queue<T>` — FIFO Queue

Circular buffer — no element shifting on dequeue.

```cpp
dsc::Queue<int> q;
q.enqueue(1);
q.enqueue(2);
int v = q.dequeue();    // O(1)
int& f = q.front();
int& b = q.back();
```

**Complexity:** `enqueue`/`dequeue` O(1) amort

---

### `dsc::Deque<T, BLOCK>` — Double-Ended Queue

Block-segmented storage — O(1) push/pop at both ends, O(1) random access.

```cpp
dsc::Deque<int> dq;
dq.push_front(0);
dq.push_back(1);
dq.pop_front();
dq.pop_back();
int v = dq[2];
```

**Complexity:** `push`/`pop` O(1) amort · `[]` O(1)

---

### `dsc::HashMap<K, V, Hash, Eq>` — Hash Map

**Robin Hood open addressing** — best-in-class cache performance.
**Backward-shift deletion** — no tombstones, no performance degradation after deletions.

```cpp
dsc::HashMap<int, int> map;
map.insert(1, 100);
map.insert_or_assign(1, 200);   // update
map[2] = 300;                   // auto-create if absent

auto opt = map.find(1);         // returns Optional<int*>
if (opt) { int val = **opt; }

map.erase(1);
map.contains(2);

for (auto [key, value] : map) { /* unordered iteration */ }
```

**Complexity:** `insert`/`erase`/`find` O(1) expected · Max load factor 87.5 %

---

### `dsc::HashSet<K, Hash, Eq>` — Hash Set

Same Robin Hood technique as `HashMap`, stores keys only.

```cpp
dsc::HashSet<int> set;
set.insert(1);
set.insert(2);
set.erase(1);
bool ok = set.contains(2);

for (const int& k : set) { /* ... */ }
```

**Complexity:** `insert`/`erase`/`contains` O(1) expected

---

### `dsc::TreeMap<K, V, Cmp>` — Ordered Map (AVL)

AVL tree — **stricter balance than Red-Black** → faster average lookups.

```cpp
dsc::TreeMap<int, int> map;
map.insert(3, 30);
map.insert(1, 10);
map.insert(2, 20);

auto opt = map.find(2);         // Optional<int*>
map[4] = 40;                    // insert or overwrite
map.erase(3);

auto min = map.min_key();       // Optional<int*>
auto max = map.max_key();

// In-order (sorted) iteration
for (auto [k, v] : map) { /* sorted by k */ }
```

**Complexity:** `insert`/`erase`/`find` O(log n)

---

### `dsc::TreeSet<K, Cmp>` — Ordered Set (AVL)

```cpp
dsc::TreeSet<int> set;
set.insert(5); set.insert(2); set.insert(8);
set.erase(2);
set.contains(5);                // O(log n)

for (const int& k : set) { /* sorted iteration */ }
```

**Complexity:** `insert`/`erase`/`contains` O(log n)

---

### `dsc::PriorityQueue<T, Cmp>` — Priority Queue

**4-ary max-heap** — half the depth of a binary heap → fewer cache misses.

```cpp
dsc::MaxHeap<int> heap;        // max at top (default)
dsc::MinHeap<int> minheap;     // min at top

heap.push(5); heap.push(1); heap.push(9);
int top = heap.top();          // 9
heap.pop();
int val = heap.pop_value();

// Build in O(n) from array
int arr[] = {3, 1, 4, 1, 5, 9};
dsc::MaxHeap<int> h2(arr, arr + 6);
```

**Complexity:** `push`/`pop` O(log n) · `top` O(1) · `build` O(n)

---

## Algorithms

### Sorting

```cpp
#include <dsc/algorithms/sort.hpp>

int arr[] = {5, 3, 8, 1, 9};
int n = 5;

// Introsort (recommended default — O(n log n) worst case)
dsc::sort(arr, arr + n);
dsc::introsort(arr, arr + n);

// With custom comparator (descending)
dsc::sort(arr, arr + n, [](int a, int b){ return a > b; });

// Stable merge sort
dsc::merge_sort(arr, arr + n);

// Heap sort (in-place, O(n log n) guaranteed)
dsc::heap_sort(arr, arr + n);

// Counting sort (O(n + k), integers only)
dsc::counting_sort(arr, arr + n);

// LSD Radix sort (O(nw), unsigned integers only)
unsigned int u[] = {42, 5, 17, 999};
dsc::radix_sort(u, u + 4);
```

### Searching

```cpp
#include <dsc/algorithms/search.hpp>

int sorted[] = {1, 3, 5, 7, 9, 11};
int n = 6;

// Binary search — O(log n), returns pointer or nullptr
int* p = dsc::binary_search(sorted, sorted + n, 7);

// Check existence without returning pointer
bool ok = dsc::binary_contains(sorted, sorted + n, 5);

// Lower/upper bound
int* lo = dsc::lower_bound(sorted, sorted + n, 5);  // first >= 5
int* hi = dsc::upper_bound(sorted, sorted + n, 5);  // first > 5

// Linear search — O(n), any range
int any[] = {9, 3, 7, 1};
int* q = dsc::linear_search(any, any + 4, 7);

// Interpolation search — O(log log n) avg, uniform distributions
int* r = dsc::interpolation_search(sorted, sorted + n, 9);
```

---

## Core Utilities

### `dsc::Optional<T>`

```cpp
dsc::Optional<int> opt = dsc::some(42);
dsc::Optional<int> empty;

if (opt) { int v = *opt; }
int v = opt.value_or(0);
opt.reset();
```

### `dsc::Hash<T>` — FNV-1a + Fibonacci mixing

Built-in specialisations for all integer types, `float`, `double`, and pointers.
Extend for custom types:

```cpp
template<>
struct dsc::Hash<MyType> {
    dsc::u64 operator()(const MyType& v) const noexcept {
        return dsc::detail::fnv1a(&v, sizeof(v));
    }
};
```

### `dsc::PoolAllocator<T, Capacity>`

```cpp
dsc::PoolAllocator<MyNode, 1024> pool;
MyNode* n = pool.allocate();
pool.construct(n, args...);
pool.destroy(n);
pool.deallocate(n);
```

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/dsc_example
```

To disable examples:

```bash
cmake -B build -DDSC_BUILD_EXAMPLES=OFF
```

---

## Design Principles

1. **Zero dependencies** — not a single `#include <std...>` or `#include <boost/...>`. Type traits, memory ops, and allocators are all hand-rolled using compiler built-ins.
2. **No exceptions** — every fallible operation returns `Optional<T>`. Deterministic performance.
3. **Move semantics everywhere** — containers never copy when they can move.
4. **Trivial type fast paths** — `__is_trivially_copyable` guards allow `memcpy`/`memmove` in hot paths, which the compiler auto-vectorises.
5. **Algorithmic soundness** — introsort guarantees O(n log n) worst case; AVL guarantees strict balance; Robin Hood hash guarantees bounded probe lengths.

---

## License

MIT © 2025 — see [LICENSE](LICENSE) for details.
