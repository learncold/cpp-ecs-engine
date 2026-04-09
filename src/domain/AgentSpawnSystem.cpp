#include "domain/AgentSpawnSystem.h"
#include "domain/MovementComponents.h"
#include <algorithm>

namespace safecrowd::domain {

    AgentSpawnSystem::AgentSpawnSystem(FacilityLayout2D layout)
        : layout_(std::move(layout)), rng_(std::random_device{}()) {
    }

    void AgentSpawnSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
        (void)step;

        if (hasSpawned_) {
            return;
        }

        auto& commands = world.commands();
        Point2D goalPos{ 19.0, 21.0 };

        for (const auto& spawnZone : layout_.spawnZones) {
            if (spawnZone.area.outline.empty()) continue;

            double minX = spawnZone.area.outline[0].x;
            double maxX = minX;
            double minY = spawnZone.area.outline[0].y;
            double maxY = minY;

            for (const auto& pt : spawnZone.area.outline) {
                minX = std::min<double>(minX, pt.x);
                maxX = std::max<double>(maxX, pt.x);
                minY = std::min<double>(minY, pt.y);
                maxY = std::max<double>(maxY, pt.y);
            }

            std::uniform_real_distribution<double> distX(minX, maxX);
            std::uniform_real_distribution<double> distY(minY, maxY);

            for (std::size_t i = 0; i < spawnZone.targetAgentCount; ++i) {
                // 엔진 API 적용: 엔티티 생성 + 컴포넌트 부착
                commands.spawnEntity(
                    Position{ Point2D{distX(rng_), distY(rng_)} },
                    Velocity{ 0.0, 0.0 },
                    Goal{ goalPos, false },
                    Agent{}
                );
            }
        }

        hasSpawned_ = true;
    }

} // namespace safecrowd::domain