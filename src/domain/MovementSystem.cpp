#include "domain/MovementSystem.h"
#include "domain/MovementComponents.h"
#include <cmath>
#include <algorithm>

namespace safecrowd::domain {

    // --- 헬퍼 함수: 벽 충돌 및 슬라이딩 계산 ---
    struct CollisionResult {
        bool collided;
        Velocity wallDirection;
    };

    CollisionResult checkWallCollision(Point2D target, Point2D p1, Point2D p2, double radius) {
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        double lineLenSq = dx * dx + dy * dy;

        if (lineLenSq == 0) return { false, {0, 0} };

        double t = ((target.x - p1.x) * dx + (target.y - p1.y) * dy) / lineLenSq;
        t = std::max(0.0, std::min(1.0, t));

        double closestX = p1.x + t * dx;
        double closestY = p1.y + t * dy;

        double distSq = std::pow(target.x - closestX, 2) + std::pow(target.y - closestY, 2);

        if (distSq < radius * radius) {
            double len = std::sqrt(lineLenSq);
            return { true, {dx / len, dy / len} };
        }
        return { false, {0, 0} };
    }

    // --- 시스템 구현 ---
    MovementSystem::MovementSystem(FacilityLayout2D layout) : layout_(std::move(layout)) {}

    void MovementSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
        (void)step;
        const double dt = 0.016;
        auto& query = world.query();

        // 1. 엔진 규칙: view를 통해 엔티티 목록 획득
        auto entities = query.view<Position, Velocity, Goal, Agent>();

        for (auto entity : entities) {
            auto& pos = query.get<Position>(entity);
            auto& vel = query.get<Velocity>(entity);
            auto& goal = query.get<Goal>(entity);
            const auto& agent = query.get<Agent>(entity);

            if (goal.reached) continue;

            double dx = goal.target.x - pos.value.x;
            double dy = goal.target.y - pos.value.y;
            double distance = std::sqrt(dx * dx + dy * dy);

            if (distance < 0.2) {
                goal.reached = true;
                vel.x = 0;
                vel.y = 0;
                continue;
            }

            double desiredVx = (dx / distance) * agent.maxSpeed;
            double desiredVy = (dy / distance) * agent.maxSpeed;

            bool collisionOccurred = false;
            Velocity slidingDir{ 0, 0 };

            // 2. 실제 헤더 구조 반영: barrier.geometry.vertices 사용
            for (const auto& barrier : layout_.barriers) {
                if (!barrier.blocksMovement) continue;

                const auto& pts = barrier.geometry.vertices;
                if (pts.size() < 2) continue;

                // 선분들 검사
                size_t segmentCount = barrier.geometry.closed ? pts.size() : pts.size() - 1;
                for (size_t i = 0; i < segmentCount; ++i) {
                    Point2D p1 = pts[i];
                    Point2D p2 = pts[(i + 1) % pts.size()];

                    auto result = checkWallCollision({ pos.value.x + desiredVx * dt, pos.value.y + desiredVy * dt }, p1, p2, agent.radius);

                    if (result.collided) {
                        collisionOccurred = true;
                        slidingDir = result.wallDirection;
                        break;
                    }
                }
                if (collisionOccurred) break;
            }

            if (collisionOccurred) {
                // 슬라이딩 로직: 벽 방향으로 속도 투영
                double dot = (desiredVx * slidingDir.x + desiredVy * slidingDir.y);
                vel.x = dot * slidingDir.x;
                vel.y = dot * slidingDir.y;
            }
            else {
                vel.x = desiredVx;
                vel.y = desiredVy;
            }

            pos.value.x += vel.x * dt;
            pos.value.y += vel.y * dt;
        }
    }

} // namespace safecrowd::domain