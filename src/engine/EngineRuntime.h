#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "engine/EngineConfig.h"
#include "engine/EngineState.h"
#include "engine/EngineStats.h"
#include "engine/EngineSystem.h"
#include "engine/FrameClock.h"

namespace ecs_engine
{
class EngineRuntime
{
public:
    explicit EngineRuntime(EngineConfig config = {}) noexcept;

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::string_view summary() const noexcept;

    [[nodiscard]] EngineState state() const noexcept;
    [[nodiscard]] const EngineConfig& config() const noexcept;
    [[nodiscard]] const EngineStats& stats() const noexcept;

    void addSystem(EngineSystem& system);
    void start() noexcept;
    void pause() noexcept;
    void resume() noexcept;
    void stop() noexcept;
    void reset() noexcept;
    void tick(double frameDeltaSeconds);

private:
    void runFixedUpdate(std::size_t stepsToRun, double stepDeltaSeconds);
    void resetStats() noexcept;

    EngineState state_{EngineState::Idle};
    FrameClock frameClock_;
    EngineStats stats_{};
    std::vector<EngineSystem*> systems_;
};
} // namespace ecs_engine
