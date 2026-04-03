#pragma once

#include <vector>

namespace safecrowd::domain {

struct Point2D {
    double x{0.0};
    double y{0.0};
};

struct LineSegment2D {
    Point2D start{};
    Point2D end{};
};

struct Polyline2D {
    std::vector<Point2D> vertices{};
    bool closed{false};
};

struct Polygon2D {
    std::vector<Point2D> outline{};
    std::vector<std::vector<Point2D>> holes{};
};

}  // namespace safecrowd::domain
