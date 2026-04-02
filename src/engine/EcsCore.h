#pragma once

#include <cstddef>

#include "engine/ComponentRegistry.h"
#include "engine/Entity.h"
#include "engine/EntityRegistry.h"

namespace safecrowd::engine {

class EcsCore {
public:
    explicit EcsCore(std::size_t maxEntityCount = 4096)
        : entityRegistry_(maxEntityCount) {}

    [[nodiscard]] Entity createEntity() {
        return entityRegistry_.allocate();
    }

    void destroyEntity(Entity entity) {
        componentRegistry_.notifyEntityDestroyed(entity);
        entityRegistry_.release(entity);
    }

    [[nodiscard]] bool isAlive(Entity entity) const noexcept {
        return entityRegistry_.isAlive(entity);
    }

    template <typename T>
    void addComponent(Entity entity, T component) {
        const ComponentType typeId = componentRegistry_.getOrRegister<T>();
        componentRegistry_.storageFor<T>().insert(entity, std::move(component));

        Signature sig = entityRegistry_.signatureOf(entity);
        sig.set(typeId);
        entityRegistry_.setSignature(entity, sig);
    }

    template <typename T>
    void removeComponent(Entity entity) {
        const auto typeId = componentRegistry_.tryTypeOf<T>();
        if (!typeId.has_value()) {
            return;
        }

        auto& storage = componentRegistry_.storageFor<T>();
        if (!storage.contains(entity)) {
            return;
        }

        storage.remove(entity);

        Signature sig = entityRegistry_.signatureOf(entity);
        sig.reset(typeId.value());
        entityRegistry_.setSignature(entity, sig);
    }

    template <typename T>
    [[nodiscard]] T& getComponent(Entity entity) {
        return componentRegistry_.storageFor<T>().get(entity);
    }

    template <typename T>
    [[nodiscard]] const T& getComponent(Entity entity) const {
        return componentRegistry_.storageFor<T>().get(entity);
    }

    template <typename T>
    [[nodiscard]] bool hasComponent(Entity entity) const {
        if (!componentRegistry_.isRegistered<T>()) {
            return false;
        }
        return componentRegistry_.storageFor<T>().contains(entity);
    }

    [[nodiscard]] EntityRegistry& entityRegistry() noexcept {
        return entityRegistry_;
    }
    [[nodiscard]] const EntityRegistry& entityRegistry() const noexcept {
        return entityRegistry_;
    }
    [[nodiscard]] ComponentRegistry& componentRegistry() noexcept {
        return componentRegistry_;
    }
    [[nodiscard]] const ComponentRegistry& componentRegistry() const noexcept {
        return componentRegistry_;
    }

private:
    EntityRegistry entityRegistry_;
    ComponentRegistry componentRegistry_;
};

}  // namespace safecrowd::engine
