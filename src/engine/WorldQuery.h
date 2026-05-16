#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "engine/EcsCore.h"

namespace safecrowd::engine {

class EngineWorld;

class WorldQuery {
public:
    template <typename... Ts>
    [[nodiscard]] std::vector<Entity> view() const {
        static_assert(sizeof...(Ts) > 0, "WorldQuery::view requires at least one component type.");

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

        const std::vector<Entity>* candidateEntities = nullptr;
        std::size_t smallestStorageSize = std::numeric_limits<std::size_t>::max();
        ([&] {
            const auto& entities = core_.componentRegistry().storageFor<Ts>().entities();
            if (entities.size() < smallestStorageSize) {
                smallestStorageSize = entities.size();
                candidateEntities = &entities;
            }
        }(), ...);

        std::vector<Entity> result;
        if (candidateEntities == nullptr) {
            return result;
        }

        result.reserve(candidateEntities->size());
        for (const auto entity : *candidateEntities) {
            const auto sig = core_.entityRegistry().signatureOf(entity);
            if ((sig & required) == required) {
                result.push_back(entity);
            }
        }
        std::sort(result.begin(), result.end(), [](Entity lhs, Entity rhs) {
            if (lhs.index != rhs.index) {
                return lhs.index < rhs.index;
            }
            return lhs.generation < rhs.generation;
        });
        return result;
    }

    template <typename... Ts, typename Fn>
    void forEach(Fn&& fn) {
        static_assert(sizeof...(Ts) > 0, "WorldQuery::forEach requires at least one component type.");

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

        if (!allRegistered) return;

        const std::vector<Entity>* candidateEntities = nullptr;
        std::size_t smallestStorageSize = std::numeric_limits<std::size_t>::max();
        ([&] {
            const auto& entities = core_.componentRegistry().storageFor<Ts>().entities();
            if (entities.size() < smallestStorageSize) {
                smallestStorageSize = entities.size();
                candidateEntities = &entities;
            }
        }(), ...);

        if (candidateEntities == nullptr) {
            return;
        }

        for (const auto entity : *candidateEntities) {
            const auto sig = core_.entityRegistry().signatureOf(entity);
            if ((sig & required) == required) {
                std::invoke(fn, entity, get<Ts>(entity)...);
            }
        }
    }

    template <typename... Ts, typename Fn>
    void forEach(Fn&& fn) const {
        static_assert(sizeof...(Ts) > 0, "WorldQuery::forEach requires at least one component type.");

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

        if (!allRegistered) return;

        const std::vector<Entity>* candidateEntities = nullptr;
        std::size_t smallestStorageSize = std::numeric_limits<std::size_t>::max();
        ([&] {
            const auto& entities = core_.componentRegistry().storageFor<Ts>().entities();
            if (entities.size() < smallestStorageSize) {
                smallestStorageSize = entities.size();
                candidateEntities = &entities;
            }
        }(), ...);

        if (candidateEntities == nullptr) {
            return;
        }

        for (const auto entity : *candidateEntities) {
            const auto sig = core_.entityRegistry().signatureOf(entity);
            if ((sig & required) == required) {
                std::invoke(fn, entity, get<Ts>(entity)...);
            }
        }
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
    friend class EngineWorld;

    explicit WorldQuery(EcsCore& core) : core_(core) {}

    EcsCore& core_;
};

}  // namespace safecrowd::engine
