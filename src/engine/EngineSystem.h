#pragma once

#include <string_view>

#include "engine/EngineStepContext.h"

namespace ecs_engine
{
class EngineSystem
{
public:
    virtual ~EngineSystem() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    virtual void update(const EngineStepContext& context) = 0;
};
} // namespace ecs_engine
