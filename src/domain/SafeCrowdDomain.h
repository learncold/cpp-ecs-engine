#pragma once

#include <string>

#include "engine/EngineRuntime.h"

namespace safecrowd
{
struct Overview
{
    std::string title;
    std::string engineSummary;
    std::string domainSummary;
};

class SafeCrowdDomain
{
public:
    [[nodiscard]] Overview buildOverview() const;

private:
    ecs_engine::EngineRuntime engine_;
};
} // namespace safecrowd
