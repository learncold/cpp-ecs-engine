#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "engine/CommandBuffer.h"
#include "engine/EcsCore.h"
#include "engine/EngineConfig.h"
#include "engine/EngineStats.h"
#include "engine/EngineSystem.h"
#include "engine/FrameClock.h"

namespace safecrowd::engine {

class EngineRuntime {
public:
    explicit EngineRuntime(EngineConfig config = {});

    void addSystem(std::unique_ptr<EngineSystem> system);

    void initialize();
    void play();
    void pause();
    void stop();
    void stepFrame(double deltaSeconds);

    EngineWorld& world() noexcept;
    const EngineWorld& world() const noexcept;
    const EngineConfig& config() const noexcept;
    const EngineStats& stats() const noexcept;
    EngineState state() const noexcept;
    std::uint64_t runIndex() const noexcept;

private:
    EngineConfig config_;
    EngineStats stats_;
    EcsCore core_;
    CommandBuffer buffer_;
    EngineWorld world_;
    FrameClock frameClock_;
    std::uint64_t runIndex_{0};
    std::vector<std::unique_ptr<EngineSystem>> systems_;
};

}  // namespace safecrowd::engine
