#pragma once

#include "engine/EngineSystem.h"
#include "domain/FacilityLayout2D.h"

namespace safecrowd::domain {

    class MovementSystem : public engine::EngineSystem {
    public:
        explicit MovementSystem(FacilityLayout2D layout);

        // 매 프레임 실행될 로직
        void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

    private:
        FacilityLayout2D layout_;
    };

} // namespace safecrowd::domain