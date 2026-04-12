#pragma once
#include <vector>
#include <cstdint>
#include "domain/Geometry2D.h"
#include "domain/metrics.h"

namespace safecrowd::engine {
    class ComponentRegistry;
}

namespace safecrowd::domain {

    struct AgentSnapshot {
        uint32_t id;
        Point2D position;
        CompressionData metrics;
    };

    struct SimulationSnapshot {
        uint64_t frameIndex = 0;
        float simulationTime = 0.0f;
        uint32_t agentCount = 0;
        std::vector<AgentSnapshot> agents;
    };

    // 네임스페이스를 safecrowd::engine으로 명시
    SimulationSnapshot buildSnapshot(const safecrowd::engine::ComponentRegistry& registry, uint64_t frame, float time);

} // namespace safecrowd::domain