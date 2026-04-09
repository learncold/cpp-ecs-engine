#pragma once

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

    // 위치 컴포넌트
    struct Position {
        Point2D value{};
    };

    // 속도/방향 컴포넌트
    struct Velocity {
        double x{ 0.0 };
        double y{ 0.0 };
    };

    // 목적지 컴포넌트
    struct Goal {
        Point2D target{};
        bool reached{ false }; // 도착 여부
    };

    // 에이전트 고유 물리 속성
    struct Agent {
        double radius{ 0.25 };   // 에이전트 크기 (반지름 25cm)
        double maxSpeed{ 1.5 };  // 최대 이동 속도 (1.5 m/s)
    };

} // namespace safecrowd::domain