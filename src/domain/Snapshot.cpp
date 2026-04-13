#include "domain/Snapshot.h"

#include "domain/AgentComponents.h"
#include "domain/Metrics.h"
#include "engine/Entity.h"
#include "engine/WorldQuery.h"

namespace safecrowd::domain {
namespace {

std::uint64_t packEntityId(engine::Entity entity) {
    return (static_cast<std::uint64_t>(entity.generation) << 32U) |
           static_cast<std::uint64_t>(entity.index);
}

bool hasCompressionMetricsForAllAgents(const engine::WorldQuery& query,
                                       const std::vector<engine::Entity>& agentEntities) {
    if (agentEntities.empty()) {
        return false;
    }

    for (const auto entity : agentEntities) {
        if (!query.contains<CompressionData>(entity)) {
            return false;
        }
    }

    return true;
}

}  // namespace

const SnapshotScalarChannel* SimulationSnapshot::findScalarChannel(std::string_view key) const noexcept {
    for (const auto& channel : scalarChannels) {
        if (channel.key == key) {
            return &channel;
        }
    }

    return nullptr;
}

const SnapshotFlagChannel* SimulationSnapshot::findFlagChannel(std::string_view key) const noexcept {
    for (const auto& channel : flagChannels) {
        if (channel.key == key) {
            return &channel;
        }
    }

    return nullptr;
}

SimulationSnapshot buildSnapshot(const engine::WorldQuery& query,
                                 std::uint64_t frame,
                                 std::uint64_t fixedStep,
                                 double simulationTime) {
    SimulationSnapshot snapshot;
    snapshot.frameIndex = frame;
    snapshot.fixedStepIndex = fixedStep;
    snapshot.simulationTime = simulationTime;

    const auto agentEntities = query.view<Position, Agent>();
    snapshot.agentCount = static_cast<std::uint32_t>(agentEntities.size());
    snapshot.agentIds.reserve(snapshot.agentCount);
    snapshot.positions.reserve(snapshot.agentCount);

    for (const auto entity : agentEntities) {
        snapshot.agentIds.push_back(packEntityId(entity));
        snapshot.positions.push_back(query.get<Position>(entity).value);
    }

    if (!hasCompressionMetricsForAllAgents(query, agentEntities)) {
        return snapshot;
    }

    SnapshotScalarChannel forceChannel{std::string(kCompressionForceChannelName), {}};
    SnapshotScalarChannel exposureChannel{std::string(kCompressionExposureChannelName), {}};
    SnapshotFlagChannel criticalChannel{std::string(kCompressionCriticalChannelName), {}};

    forceChannel.values.reserve(snapshot.agentCount);
    exposureChannel.values.reserve(snapshot.agentCount);
    criticalChannel.values.reserve(snapshot.agentCount);

    for (const auto entity : agentEntities) {
        const auto& metrics = query.get<CompressionData>(entity);
        forceChannel.values.push_back(metrics.force);
        exposureChannel.values.push_back(metrics.exposure);
        criticalChannel.values.push_back(metrics.isCritical ? 1U : 0U);
    }

    snapshot.scalarChannels.push_back(std::move(forceChannel));
    snapshot.scalarChannels.push_back(std::move(exposureChannel));
    snapshot.flagChannels.push_back(std::move(criticalChannel));

    return snapshot;
}

}  // namespace safecrowd::domain
