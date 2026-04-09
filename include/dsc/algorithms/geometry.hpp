#pragma once

#include "../core/primitives.hpp"
#include "../core/traits.hpp"
#include "../containers/array.hpp"
#include "sort.hpp"

// ── Geometry algorithms ───────────────────────────────────────────────────────
// 2D computational geometry primitives.

namespace dsc {

// ── Point structure ───────────────────────────────────────────────────────────
template<typename T>
struct Point {
    T x, y;
    Point(T x = T{}, T y = T{}) : x(x), y(y) {}
    Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }
    T cross(const Point& o) const { return x * o.y - y * o.x; }
};

// ── Convex hull ───────────────────────────────────────────────────────────────
// Computes the convex hull of a set of points using Graham scan.
/// @brief Computes the convex hull of a set of 2D points using Graham scan.
/// @tparam T Coordinate type.
/// @param points Array of points.
/// @return Array of points on the convex hull in counter-clockwise order.
/// @complexity O(n log n).
/// @note Points must be in general position (no three collinear).
template<typename T>
Array<Point<T>> convex_hull(Array<Point<T>> points) {
    if (points.size() <= 1) return points;

    // Find the bottommost point
    usize min_idx = 0;
    for (usize i = 1; i < points.size(); ++i) {
        if (points[i].y < points[min_idx].y ||
            (points[i].y == points[min_idx].y && points[i].x < points[min_idx].x)) {
            min_idx = i;
        }
    }
    traits::swap(points[0], points[min_idx]);

    Point<T> p0 = points[0];

    // Sort by polar angle with p0
    introsort(points.data() + 1, points.data() + points.size(),
              [&](const Point<T>& a, const Point<T>& b) {
                  Point<T> va = a - p0, vb = b - p0;
                  T cross = va.cross(vb);
                  if (cross == 0) return (a.x - p0.x) * (a.x - p0.x) + (a.y - p0.y) * (a.y - p0.y) <
                                         (b.x - p0.x) * (b.x - p0.x) + (b.y - p0.y) * (b.y - p0.y);
                  return cross > 0;
              });

    // Build hull
    Array<Point<T>> hull;
    for (const auto& p : points) {
        while (hull.size() >= 2) {
            Point<T> a = hull[hull.size() - 2], b = hull.back(), c = p;
            if ((b - a).cross(c - b) <= 0) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }
    return hull;
}

} // namespace dsc