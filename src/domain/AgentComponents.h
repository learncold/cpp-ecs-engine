#pragma once

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

struct Position {
    Point2D value;
};

struct Agent {
    float radius{0.25f};
    float maxSpeed{1.5f};
};

struct Velocity {
    Point2D value;
};

}  // namespace safecrowd::domain
