// dsc — basic usage example
// Build: c++ -std=c++20 -O2 -I../include -o example basic_usage.cpp

#include <dsc/dsc.hpp>

// ── Tiny write helper (no <stdio.h> / <iostream>) ────────────────────────────
// Uses the POSIX write() syscall via inline asm or a compiler built-in.
// This keeps the example truly std-free.
namespace io {

// Convert integer to string in buf, returns length
static int itoa(long long v, char* buf) noexcept {
    if (v == 0) { buf[0] = '0'; buf[1] = '\n'; return 2; }
    bool neg = v < 0;
    if (neg) v = -v;
    char tmp[22]; int len = 0;
    while (v > 0) { tmp[len++] = '0' + (v % 10); v /= 10; }
    if (neg) tmp[len++] = '-';
    int out = 0;
    for (int i = len - 1; i >= 0; --i) buf[out++] = tmp[i];
    buf[out++] = '\n';
    return out;
}

static void print_int(long long v) noexcept {
    char buf[24];
    int  n = itoa(v, buf);
    // write(1, buf, n)
#if defined(__linux__)
    __asm__ volatile(
        "syscall"
        : : "a"(1), "D"(1), "S"(buf), "d"(n)
        : "rcx", "r11", "memory"
    );
#elif defined(__APPLE__)
    __asm__ volatile(
        "syscall"
        : : "a"(0x2000004), "D"(1), "S"(buf), "d"(n)
        : "rcx", "r11", "memory"
    );
#else
    // Fallback: operator<< on a file descriptor isn't available without libc.
    // This branch is intentionally empty — extend for your platform.
    (void)buf; (void)n;
#endif
}

static void print_str(const char* s) noexcept {
    dsc::usize len = 0;
    while (s[len]) ++len;
#if defined(__linux__)
    __asm__ volatile("syscall" : : "a"(1), "D"(1), "S"(s), "d"(len) : "rcx","r11","memory");
#elif defined(__APPLE__)
    __asm__ volatile("syscall" : : "a"(0x2000004), "D"(1), "S"(s), "d"(len) : "rcx","r11","memory");
#else
    (void)s; (void)len;
#endif
}

} // namespace io

using namespace dsc;

int main() {
    // ── Array ────────────────────────────────────────────────────────────────
    io::print_str("=== Array ===\n");
    Array<int> arr;
    for (int i = 0; i < 10; ++i) arr.push_back(i * i);
    for (int x : arr) { io::print_int(x); }

    // ── Sort ─────────────────────────────────────────────────────────────────
    io::print_str("=== Introsort (descending) ===\n");
    int nums[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    introsort(nums, nums + 10, [](int a, int b){ return a > b; });
    for (int x : nums) { io::print_int(x); }

    // ── Binary search ────────────────────────────────────────────────────────
    io::print_str("=== Binary search ===\n");
    int sorted[] = {1, 3, 5, 7, 9, 11, 13};
    int* found = binary_search(sorted, sorted + 7, 7);
    io::print_str(found ? "Found 7\n" : "Not found\n");

    // ── Stack ────────────────────────────────────────────────────────────────
    io::print_str("=== Stack ===\n");
    Stack<int> stk;
    stk.push(10); stk.push(20); stk.push(30);
    while (!stk.empty()) { io::print_int(stk.pop_value()); }

    // ── Queue ────────────────────────────────────────────────────────────────
    io::print_str("=== Queue ===\n");
    Queue<int> q;
    q.enqueue(1); q.enqueue(2); q.enqueue(3);
    while (!q.empty()) { io::print_int(q.dequeue()); }

    // ── Linked list ──────────────────────────────────────────────────────────
    io::print_str("=== LinkedList ===\n");
    LinkedList<int> lst;
    lst.push_back(100); lst.push_back(200); lst.push_front(50);
    for (int x : lst) { io::print_int(x); }

    // ── HashMap ──────────────────────────────────────────────────────────────
    io::print_str("=== HashMap ===\n");
    HashMap<int, int> map;
    map.insert(1, 10); map.insert(2, 20); map.insert(3, 30);
    for (auto [k, v] : map) { io::print_int(k); io::print_int(v); }

    // ── HashSet ──────────────────────────────────────────────────────────────
    io::print_str("=== HashSet contains(2): ===\n");
    HashSet<int> set;
    set.insert(1); set.insert(2); set.insert(3);
    io::print_str(set.contains(2) ? "yes\n" : "no\n");

    // ── TreeMap (sorted order) ────────────────────────────────────────────────
    io::print_str("=== TreeMap (sorted) ===\n");
    TreeMap<int, int> tmap;
    tmap.insert(3, 30); tmap.insert(1, 10); tmap.insert(2, 20);
    for (auto [k, v] : tmap) { io::print_int(k); }

    // ── PriorityQueue ────────────────────────────────────────────────────────
    io::print_str("=== MaxHeap ===\n");
    MaxHeap<int> heap;
    heap.push(4); heap.push(1); heap.push(7); heap.push(3);
    while (!heap.empty()) { io::print_int(heap.pop_value()); }

    // ── Radix sort ───────────────────────────────────────────────────────────
    io::print_str("=== Radix sort ===\n");
    unsigned int r[] = {42, 5, 17, 999, 1, 100};
    radix_sort(r, r + 6);
    for (unsigned int x : r) { io::print_int(static_cast<long long>(x)); }

    // ── Optional ─────────────────────────────────────────────────────────────
    io::print_str("=== Optional ===\n");
    Optional<int> opt = some(42);
    io::print_str(opt.has_value() ? "has value\n" : "empty\n");
    io::print_int(*opt);

    return 0;
}
