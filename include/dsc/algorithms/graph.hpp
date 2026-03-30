#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../core/optional.hpp"
#include "../containers/array.hpp"
#include "../containers/queue.hpp"
#include "../containers/stack.hpp"
#include "../containers/priority_queue.hpp"
#include "../containers/union_find.hpp"

// ── Graph algorithms ──────────────────────────────────────────────────────────
// All algorithms accept an adjacency list: Array<Array<Edge>> where each
// edge is a struct { usize to; T weight; }.
// Vertices are numbered 0 … n-1.

namespace dsc {

// ── Edge types ────────────────────────────────────────────────────────────────
struct Edge {
    usize to;
};

template<typename W>
struct WeightedEdge {
    usize to;
    W     weight;
};

// Infinity sentinel for shortest-path algorithms
template<typename T> constexpr T INF = static_cast<T>(-1) >> 1;  // max positive value

// ── BFS — Breadth-First Search ────────────────────────────────────────────────
// Visits all vertices reachable from `src` in O(V + E).
// Returns the distance (hop count) from src to each vertex, or INF<usize> if unreachable.
template<typename EdgeT>
Array<usize> bfs(const Array<Array<EdgeT>>& graph, usize src) {
    usize n = graph.size();
    Array<usize> dist(n, INF<usize>);
    dist[src] = 0;
    Queue<usize> q;
    q.enqueue(src);
    while (!q.empty()) {
        usize u = q.dequeue();
        for (const auto& e : graph[u]) {
            if (dist[e.to] == INF<usize>) {
                dist[e.to] = dist[u] + 1;
                q.enqueue(e.to);
            }
        }
    }
    return dist;
}

// BFS with parent tracking for path reconstruction
template<typename EdgeT>
Array<usize> bfs_parents(const Array<Array<EdgeT>>& graph, usize src) {
    usize n = graph.size();
    Array<usize> parent(n, INF<usize>);
    Array<bool>  visited(n, false);
    visited[src] = true;
    parent[src]  = src;
    Queue<usize> q;
    q.enqueue(src);
    while (!q.empty()) {
        usize u = q.dequeue();
        for (const auto& e : graph[u]) {
            if (!visited[e.to]) {
                visited[e.to] = true;
                parent[e.to]  = u;
                q.enqueue(e.to);
            }
        }
    }
    return parent;
}

// Reconstruct path from src to dst given parent array; empty if unreachable
Array<usize> reconstruct_path(const Array<usize>& parent, usize src, usize dst) {
    Array<usize> path;
    if (parent[dst] == INF<usize>) return path;
    for (usize v = dst; v != src; v = parent[v]) path.push_back(v);
    path.push_back(src);
    // Reverse
    usize l = 0, r = path.size() - 1;
    while (l < r) { traits::swap(path[l++], path[r--]); }
    return path;
}

// ── DFS — Depth-First Search ──────────────────────────────────────────────────
// Iterative DFS to avoid stack overflow on deep graphs.
// Returns vertices in DFS finish order.
template<typename EdgeT>
Array<usize> dfs(const Array<Array<EdgeT>>& graph, usize src) {
    usize n = graph.size();
    Array<bool>  visited(n, false);
    Array<usize> order;
    Stack<usize> stk;
    stk.push(src);
    while (!stk.empty()) {
        usize u = stk.pop_value();
        if (visited[u]) continue;
        visited[u] = true;
        order.push_back(u);
        for (const auto& e : graph[u]) {
            if (!visited[e.to]) stk.push(e.to);
        }
    }
    return order;
}

// ── Topological Sort (Kahn's algorithm — BFS-based) ───────────────────────────
// Valid only for DAGs. Returns vertices in topological order.
// If the graph has a cycle, the returned array has fewer than n elements.
template<typename EdgeT>
Array<usize> topological_sort(const Array<Array<EdgeT>>& graph) {
    usize n = graph.size();
    Array<usize> indegree(n, 0u);
    for (usize u = 0; u < n; ++u)
        for (const auto& e : graph[u]) ++indegree[e.to];

    Queue<usize> q;
    for (usize u = 0; u < n; ++u) if (indegree[u] == 0) q.enqueue(u);

    Array<usize> order;
    while (!q.empty()) {
        usize u = q.dequeue();
        order.push_back(u);
        for (const auto& e : graph[u]) {
            if (--indegree[e.to] == 0) q.enqueue(e.to);
        }
    }
    return order;  // size < n means cycle exists
}

// ── Dijkstra's shortest path ──────────────────────────────────────────────────
// Single-source shortest paths on graphs with non-negative edge weights.
// O((V + E) log V) using a 4-ary min-heap.
template<typename W>
Array<W> dijkstra(const Array<Array<WeightedEdge<W>>>& graph, usize src) {
    usize n = graph.size();
    Array<W> dist(n, INF<W>);
    dist[src] = W{};

    struct PQ_Node { W dist; usize v; };
    MinHeap<PQ_Node, decltype([](const PQ_Node& a, const PQ_Node& b){ return a.dist > b.dist; })> pq;
    pq.push({W{}, src});

    while (!pq.empty()) {
        auto [d, u] = pq.pop_value();
        if (d > dist[u]) continue;  // stale entry
        for (const auto& e : graph[u]) {
            W nd = dist[u] + e.weight;
            if (nd < dist[e.to]) {
                dist[e.to] = nd;
                pq.push({nd, e.to});
            }
        }
    }
    return dist;
}

// Dijkstra with parent tracking
template<typename W>
struct DijkstraResult { Array<W> dist; Array<usize> parent; };

template<typename W>
DijkstraResult<W> dijkstra_parents(const Array<Array<WeightedEdge<W>>>& graph, usize src) {
    usize n = graph.size();
    Array<W>     dist(n, INF<W>);
    Array<usize> parent(n, INF<usize>);
    dist[src] = W{}; parent[src] = src;

    struct PQ_Node { W dist; usize v; };
    MinHeap<PQ_Node, decltype([](const PQ_Node& a, const PQ_Node& b){ return a.dist > b.dist; })> pq;
    pq.push({W{}, src});

    while (!pq.empty()) {
        auto [d, u] = pq.pop_value();
        if (d > dist[u]) continue;
        for (const auto& e : graph[u]) {
            W nd = dist[u] + e.weight;
            if (nd < dist[e.to]) {
                dist[e.to]   = nd;
                parent[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }
    return {traits::move(dist), traits::move(parent)};
}

// ── Bellman-Ford ──────────────────────────────────────────────────────────────
// Single-source shortest paths that handles negative edge weights.
// Detects negative cycles. O(VE).
template<typename W>
struct BFEdge { usize from, to; W weight; };

template<typename W>
struct BellmanFordResult { Array<W> dist; bool has_negative_cycle; };

template<typename W>
BellmanFordResult<W> bellman_ford(const Array<BFEdge<W>>& edges, usize n, usize src) {
    Array<W> dist(n, INF<W>);
    dist[src] = W{};

    // Relax all edges V-1 times
    for (usize i = 1; i < n; ++i) {
        for (const auto& e : edges) {
            if (dist[e.from] != INF<W> && dist[e.from] + e.weight < dist[e.to])
                dist[e.to] = dist[e.from] + e.weight;
        }
    }

    // V-th relaxation detects negative cycle
    bool neg_cycle = false;
    for (const auto& e : edges) {
        if (dist[e.from] != INF<W> && dist[e.from] + e.weight < dist[e.to]) {
            neg_cycle = true; break;
        }
    }

    return {traits::move(dist), neg_cycle};
}

// ── Floyd-Warshall ────────────────────────────────────────────────────────────
// All-pairs shortest paths. O(V³). Returns V×V distance matrix (flat array).
// dist[u*n + v] = shortest path u→v; INF<W> if unreachable.
template<typename W>
Array<W> floyd_warshall(usize n, const Array<BFEdge<W>>& edges) {
    Array<W> d(n * n, INF<W>);
    for (usize i = 0; i < n; ++i) d[i*n + i] = W{};
    for (const auto& e : edges) {
        if (e.weight < d[e.from*n + e.to]) d[e.from*n + e.to] = e.weight;
    }
    for (usize k = 0; k < n; ++k)
        for (usize i = 0; i < n; ++i)
            for (usize j = 0; j < n; ++j)
                if (d[i*n+k] != INF<W> && d[k*n+j] != INF<W>)
                    if (d[i*n+k] + d[k*n+j] < d[i*n+j])
                        d[i*n+j] = d[i*n+k] + d[k*n+j];
    return d;
}

// ── Kruskal's MST ─────────────────────────────────────────────────────────────
// Minimum spanning tree for undirected weighted graphs. O(E log E).
// Returns edges in the MST; total weight can be summed by the caller.
template<typename W>
struct UndirectedEdge { usize u, v; W weight; };

template<typename W>
Array<UndirectedEdge<W>> kruskal(usize n, Array<UndirectedEdge<W>> edges) {
    // Sort edges by weight (introsort)
    introsort(edges.data(), edges.data() + edges.size(),
              [](const UndirectedEdge<W>& a, const UndirectedEdge<W>& b){ return a.weight < b.weight; });

    UnionFind uf(n);
    Array<UndirectedEdge<W>> mst;
    for (const auto& e : edges) {
        if (uf.unite(e.u, e.v)) {
            mst.push_back(e);
            if (mst.size() == n - 1) break;
        }
    }
    return mst;
}

// ── Prim's MST ────────────────────────────────────────────────────────────────
// Minimum spanning tree. O((V + E) log V). Better than Kruskal for dense graphs.
template<typename W>
Array<UndirectedEdge<W>> prim(const Array<Array<WeightedEdge<W>>>& graph) {
    usize n = graph.size();
    Array<W>     key(n, INF<W>);
    Array<usize> parent(n, INF<usize>);
    Array<bool>  in_mst(n, false);

    key[0] = W{};

    struct PQNode { W key; usize v; };
    MinHeap<PQNode, decltype([](const PQNode& a, const PQNode& b){ return a.key > b.key; })> pq;
    pq.push({W{}, 0});

    Array<UndirectedEdge<W>> mst;

    while (!pq.empty()) {
        auto [k, u] = pq.pop_value();
        if (in_mst[u]) continue;
        in_mst[u] = true;
        if (parent[u] != INF<usize>) mst.push_back({parent[u], u, k});

        for (const auto& e : graph[u]) {
            if (!in_mst[e.to] && e.weight < key[e.to]) {
                key[e.to]    = e.weight;
                parent[e.to] = u;
                pq.push({e.weight, e.to});
            }
        }
    }
    return mst;
}

// ── Connected components ──────────────────────────────────────────────────────
// Returns component ID for each vertex. O(V + E).
template<typename EdgeT>
Array<usize> connected_components(const Array<Array<EdgeT>>& graph) {
    usize n = graph.size();
    Array<usize> comp(n, INF<usize>);
    usize cid = 0;
    for (usize src = 0; src < n; ++src) {
        if (comp[src] != INF<usize>) continue;
        Stack<usize> stk;
        stk.push(src);
        while (!stk.empty()) {
            usize u = stk.pop_value();
            if (comp[u] != INF<usize>) continue;
            comp[u] = cid;
            for (const auto& e : graph[u]) if (comp[e.to] == INF<usize>) stk.push(e.to);
        }
        ++cid;
    }
    return comp;
}

// ── Cycle detection (directed graph) ─────────────────────────────────────────
template<typename EdgeT>
bool has_cycle_directed(const Array<Array<EdgeT>>& graph) {
    usize n = graph.size();
    Array<u8> color(n, 0);  // 0=white 1=gray 2=black
    bool found = false;

    auto dfs_visit = [&](auto& self, usize u) -> void {
        if (found) return;
        color[u] = 1;
        for (const auto& e : graph[u]) {
            if (color[e.to] == 1) { found = true; return; }
            if (color[e.to] == 0) self(self, e.to);
        }
        color[u] = 2;
    };

    for (usize i = 0; i < n; ++i)
        if (color[i] == 0) dfs_visit(dfs_visit, i);

    return found;
}

// ── Bipartite check ───────────────────────────────────────────────────────────
template<typename EdgeT>
bool is_bipartite(const Array<Array<EdgeT>>& graph) {
    usize n = graph.size();
    Array<i32> color(n, -1);
    for (usize src = 0; src < n; ++src) {
        if (color[src] != -1) continue;
        color[src] = 0;
        Queue<usize> q;
        q.enqueue(src);
        while (!q.empty()) {
            usize u = q.dequeue();
            for (const auto& e : graph[u]) {
                if (color[e.to] == -1) {
                    color[e.to] = 1 - color[u];
                    q.enqueue(e.to);
                } else if (color[e.to] == color[u]) return false;
            }
        }
    }
    return true;
}

} // namespace dsc
