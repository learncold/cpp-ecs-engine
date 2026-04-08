#pragma once

#include "domain/FacilityLayout2D.h"

namespace safecrowd::domain {

    class DemoFixtureService {
    public:
        // Sprint 1 데모를 위한 20x20 크기의 고정 맵을 생성
        FacilityLayout2D createSprint1DemoLayout() const;
    };

}  // namespace safecrowd::domain