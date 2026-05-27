#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/Geometry2D.h"

namespace safecrowd::domain {

struct SimulationAgentFrame {
    std::uint64_t id{0};
    Point2D position{};
    Point2D velocity{};
    double radius{static_cast<double>(kDefaultAgentRadiusMeters)};
    std::string floorId{};
    bool stalled{false};
};

struct SimulationFrame {
    double elapsedSeconds{0.0};
    bool complete{false};
    std::size_t totalAgentCount{0};
    std::size_t evacuatedAgentCount{0};
    std::vector<SimulationAgentFrame> agents{};
};

}  // namespace safecrowd::domain
