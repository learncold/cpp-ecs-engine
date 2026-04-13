#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::engine {
class WorldQuery;
}

namespace safecrowd::domain {

inline constexpr std::string_view kCompressionForceChannelName = "compression.force";
inline constexpr std::string_view kCompressionExposureChannelName = "compression.exposure";
inline constexpr std::string_view kCompressionCriticalChannelName = "compression.critical";

struct SnapshotScalarChannel {
    std::string key;
    std::vector<float> values;
};

struct SnapshotFlagChannel {
    std::string key;
    std::vector<std::uint8_t> values;
};

struct SimulationSnapshot {
    std::uint64_t frameIndex{0};
    std::uint64_t fixedStepIndex{0};
    double simulationTime{0.0};
    std::uint32_t agentCount{0};
    std::vector<std::uint64_t> agentIds;
    std::vector<Point2D> positions;
    std::vector<SnapshotScalarChannel> scalarChannels;
    std::vector<SnapshotFlagChannel> flagChannels;

    [[nodiscard]] const SnapshotScalarChannel* findScalarChannel(std::string_view key) const noexcept;
    [[nodiscard]] const SnapshotFlagChannel* findFlagChannel(std::string_view key) const noexcept;
};

SimulationSnapshot buildSnapshot(const engine::WorldQuery& query,
                                 std::uint64_t frame,
                                 std::uint64_t fixedStep,
                                 double simulationTime);

}  // namespace safecrowd::domain
