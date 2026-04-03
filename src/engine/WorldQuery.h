#pragma once

#include <vector>

#include "engine/EcsCore.h"

namespace safecrowd::engine {

class WorldQuery {
public:
    explicit WorldQuery(EcsCore& core) : core_(core) {}

    template <typename... Ts>
    [[nodiscard]] std::vector<Entity> view() const {
        Signature required{};
        bool allRegistered = true;
        ([&] {
            const auto id = core_.componentRegistry().tryTypeOf<Ts>();
            if (!id.has_value()) {
                allRegistered = false;
            } else {
                required.set(id.value());
            }
        }(), ...);

        if (!allRegistered) return {};

        std::vector<Entity> result;
        core_.entityRegistry().eachAlive([&](Entity entity, const Signature& sig) {
            if ((sig & required) == required) {
                result.push_back(entity);
            }
        });
        return result;
    }

    template <typename T>
    [[nodiscard]] bool contains(Entity entity) const {
        return core_.hasComponent<T>(entity);
    }

    template <typename T>
    [[nodiscard]] T& get(Entity entity) {
        return core_.getComponent<T>(entity);
    }

    template <typename T>
    [[nodiscard]] const T& get(Entity entity) const {
        return core_.getComponent<T>(entity);
    }

private:
    EcsCore& core_;
};

}  // namespace safecrowd::engine
