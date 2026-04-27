#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

struct SimulationAgentFrame {
    std::uint64_t id{0};
    Point2D position{};
    Point2D velocity{};
    double radius{0.25};
};

struct SimulationFrame {
    double elapsedSeconds{0.0};
    bool complete{false};
    std::size_t totalAgentCount{0};
    std::size_t evacuatedAgentCount{0};
    std::vector<SimulationAgentFrame> agents{};
};

}  // namespace safecrowd::domain
