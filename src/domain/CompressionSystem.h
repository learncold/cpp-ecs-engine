#pragma once

#include "engine/ComponentRegistry.h" 

namespace safecrowd::domain {

    // 시스템 클래스 선언
    class CompressionSystem {
    public:
        // 업데이트 함수 선언
        void update(engine::ComponentRegistry& registry, float dt);
    };

} // namespace safecrowd::domain