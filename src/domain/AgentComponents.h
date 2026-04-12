#pragma once
#include "domain/Geometry2D.h"

namespace safecrowd::domain {

    // 에이전트의 위치를 나타내는 컴포넌트
    struct Position {
        Point2D value;
    };

    // 에이전트의 고유 특성을 나타내는 컴포넌트
    struct Agent {
        float radius = 0.25f;
        float maxSpeed = 1.5f;
    };

    struct Velocity {
        Point2D value;
    };

} // namespace safecrowd::domain