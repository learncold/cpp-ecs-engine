#pragma once

#include "engine/EngineSystem.h"

namespace safecrowd::domain {

class CompressionSystem final : public engine::EngineSystem {
public:
    explicit CompressionSystem(double timeStepSeconds);

    void update(engine::EngineWorld& world,
                const engine::EngineStepContext& step) override;

private:
    float timeStepSeconds_{0.0f};
};

}  // namespace safecrowd::domain
