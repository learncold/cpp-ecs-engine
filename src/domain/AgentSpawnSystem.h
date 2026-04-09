#pragma once

#include <random>
#include "engine/EngineSystem.h"
#include "domain/FacilityLayout2D.h"

namespace safecrowd::domain {

    class AgentSpawnSystem : public engine::EngineSystem {
    public:
        // 데모 맵 데이터를 받아 초기화
        explicit AgentSpawnSystem(FacilityLayout2D layout);

        // 엔진 루프에서 매 프레임 호출되지만, 스폰은 딱 한 번만 실행되도록 제어
        void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

    private:
        FacilityLayout2D layout_;
        bool hasSpawned_{ false }; // 이미 스폰했는지 확인하는 플래그
        std::mt19937 rng_;       // 랜덤 좌표 생성을 위한 난수 생성기
    };

} // namespace safecrowd::domain