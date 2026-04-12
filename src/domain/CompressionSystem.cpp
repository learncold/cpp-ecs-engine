#include "domain/CompressionSystem.h"
#include "domain/FacilityLayout2D.h"
#include "domain/AgentComponents.h" 
#include "domain/metrics.h"
#include <algorithm>
#include <cmath>

namespace safecrowd::domain {

    static float distanceBetween(const Point2D& p1, const Point2D& p2) {
        float dx = static_cast<float>(p1.x - p2.x);
        float dy = static_cast<float>(p1.y - p2.y);
        return std::sqrt(dx * dx + dy * dy);
    }

    static float distancePointToSegment(const Point2D& p, const Point2D& a, const Point2D& b) {
        float l2 = static_cast<float>(std::pow(b.x - a.x, 2) + std::pow(b.y - a.y, 2));
        if (l2 == 0.0f) return distanceBetween(p, a);

        float t = std::clamp(static_cast<float>(((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2), 0.0f, 1.0f);
        Point2D projection = { a.x + t * (b.x - a.x), a.y + t * (b.y - a.y) };
        return distanceBetween(p, projection);
    }

    void CompressionSystem::update(engine::ComponentRegistry& registry, float dt) {
        // 필요한 스토리지들을 로드
        auto& posStorage = registry.storageFor<Position>();
        auto& agentStorage = registry.storageFor<Agent>();
        auto& compStorage = registry.storageFor<CompressionData>();

        // Barrier2D 스토리지가 있는지 확인
        if (!registry.isRegistered<Barrier2D>()) return;
        auto& barrierStorage = registry.storageFor<Barrier2D>();

        // Position을 가진 모든 엔티티를 순회
        for (const auto& entity : posStorage.getEntities()) {
            // Agent와 CompressionData 컴포넌트가 모두 있는지 확인
            if (!agentStorage.contains(entity) || !compStorage.contains(entity)) continue;

            const auto& pos = posStorage.get(entity);
            const auto& agent = agentStorage.get(entity);
            auto& compression = compStorage.get(entity);

            float currentForce = 0.0f;

            // [군중 간 압박]
            for (const auto& otherEntity : posStorage.getEntities()) {
                if (entity.index == otherEntity.index && entity.generation == otherEntity.generation) continue;
                if (!agentStorage.contains(otherEntity)) continue;

                const auto& otherPos = posStorage.get(otherEntity);
                const auto& otherAgent = agentStorage.get(otherEntity);

                float dist = distanceBetween(pos.value, otherPos.value);
                float combinedRadius = agent.radius + otherAgent.radius;

                if (dist < combinedRadius) {
                    currentForce += (combinedRadius - dist);
                }
            }

            // [벽/장애물 압박]
            for (const auto& barrierEntity : barrierStorage.getEntities()) {
                const auto& barrier = barrierStorage.get(barrierEntity);
                const auto& vertices = barrier.geometry.vertices;
                if (vertices.size() < 2) continue;

                for (size_t i = 0; i < vertices.size() - 1; ++i) {
                    float distToWall = distancePointToSegment(pos.value, vertices[i], vertices[i + 1]);
                    if (distToWall < agent.radius) {
                        currentForce += (agent.radius - distToWall);
                    }
                }
                if (barrier.geometry.closed) {
                    float distToWall = distancePointToSegment(pos.value, vertices.back(), vertices.front());
                    if (distToWall < agent.radius) {
                        currentForce += (agent.radius - distToWall);
                    }
                }
            }

            compression.force = currentForce;

            // [고위험 상태 업데이트]
            const float FORCE_THRESHOLD = 0.5f;
            if (compression.force > FORCE_THRESHOLD) {
                compression.exposure += dt;
            }
            else {
                compression.exposure = std::max(0.0f, compression.exposure - dt * 0.5f);
            }

            const float EXPOSURE_THRESHOLD = 2.0f;
            compression.isCritical = (compression.force > FORCE_THRESHOLD) &&
                (compression.exposure > EXPOSURE_THRESHOLD);
        }
    }

} // namespace safecrowd::domain