#include "domain/CompressionSystem.h"

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/Metrics.h"

#include <algorithm>
#include <cmath>

namespace safecrowd::domain {
namespace {

constexpr float kForceThreshold = 0.5f;
constexpr float kExposureThreshold = 2.0f;

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    const double dx = lhs.x - rhs.x;
    const double dy = lhs.y - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

double distancePointToSegment(const Point2D& point, const Point2D& start, const Point2D& end) {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = (dx * dx) + (dy * dy);

    if (lengthSquared == 0.0) {
        return distanceBetween(point, start);
    }

    const double t = std::clamp(
        (((point.x - start.x) * dx) + ((point.y - start.y) * dy)) / lengthSquared,
        0.0,
        1.0);
    const Point2D projection{
        .x = start.x + (t * dx),
        .y = start.y + (t * dy),
    };
    return distanceBetween(point, projection);
}

double barrierCompression(const Barrier2D& barrier, const Point2D& position, double radius) {
    if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
        return 0.0;
    }

    double force = 0.0;
    const auto& vertices = barrier.geometry.vertices;

    for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
        const double distance = distancePointToSegment(position, vertices[index], vertices[index + 1]);
        if (distance < radius) {
            force += radius - distance;
        }
    }

    if (barrier.geometry.closed) {
        const double distance = distancePointToSegment(position, vertices.back(), vertices.front());
        if (distance < radius) {
            force += radius - distance;
        }
    }

    return force;
}

}  // namespace

CompressionSystem::CompressionSystem(double timeStepSeconds)
    : timeStepSeconds_(static_cast<float>(std::max(0.0, timeStepSeconds))) {
}

void CompressionSystem::update(engine::EngineWorld& world,
                               const engine::EngineStepContext& step) {
    (void)step;

    auto& query = world.query();
    const auto agentEntities = query.view<Position, Agent, CompressionData>();
    const auto barrierEntities = query.view<Barrier2D>();

    for (const auto entity : agentEntities) {
        const auto& position = query.get<Position>(entity);
        const auto& agent = query.get<Agent>(entity);
        auto& compression = query.get<CompressionData>(entity);

        double currentForce = 0.0;

        for (const auto otherEntity : agentEntities) {
            if (otherEntity == entity) {
                continue;
            }

            const auto& otherPosition = query.get<Position>(otherEntity);
            const auto& otherAgent = query.get<Agent>(otherEntity);
            const double distance = distanceBetween(position.value, otherPosition.value);
            const double combinedRadius = static_cast<double>(agent.radius + otherAgent.radius);

            if (distance < combinedRadius) {
                currentForce += combinedRadius - distance;
            }
        }

        for (const auto barrierEntity : barrierEntities) {
            currentForce += barrierCompression(
                query.get<Barrier2D>(barrierEntity),
                position.value,
                static_cast<double>(agent.radius));
        }

        compression.force = static_cast<float>(currentForce);
        if (compression.force > kForceThreshold) {
            compression.exposure += timeStepSeconds_;
        }

        compression.isCritical =
            compression.force > kForceThreshold &&
            compression.exposure >= kExposureThreshold;
    }
}

}  // namespace safecrowd::domain
