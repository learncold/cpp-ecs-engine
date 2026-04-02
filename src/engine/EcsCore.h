#pragma once

#include <cstddef>
#include <stdexcept>

#include "engine/ComponentRegistry.h"
#include "engine/Entity.h"
#include "engine/EntityRegistry.h"

namespace safecrowd::engine {

class EcsCore {
public:
    explicit EcsCore(std::size_t maxEntityCount = 4096)
        : entityRegistry_(maxEntityCount),
          maxEntityCount_(maxEntityCount) {}

    ~EcsCore() {
        // Destructors must not throw.
        try {
            shutdown();
        } catch (...) {
        }
    }

    void shutdown() {
        componentRegistry_.shutdown();
        entityRegistry_ = EntityRegistry(maxEntityCount_);
    }

    [[nodiscard]] Entity createEntity() {
        return entityRegistry_.allocate();
    }

    void destroyEntity(Entity entity) {
        // Important order: call storage cleanup with the original generation,
        // then release the entity handle (which increments generation).
        componentRegistry_.entityDestroyed(entity);
        entityRegistry_.release(entity);
    }

    template <typename T>
    void registerType() {
        componentRegistry_.registerType<T>();
    }

    template <typename T>
    [[nodiscard]] ComponentType componentType() const {
        // ComponentRegistry::componentType<T>() throws if not registered.
        return componentRegistry_.componentType<T>();
    }

    template <typename T>
    void addComponent(Entity entity, const T& component) {
        // Must be explicitly registered; do not auto-register.
        const ComponentType id = componentRegistry_.componentType<T>();

        auto& storage = componentRegistry_.storage<T>();
        storage.insert(entity, component);

        auto signature = entityRegistry_.signatureOf(entity);
        signature.set(id);
        entityRegistry_.setSignature(entity, signature);
    }

    template <typename T>
    void addComponent(Entity entity, T&& component) {
        const ComponentType id = componentRegistry_.componentType<T>();

        auto& storage = componentRegistry_.storage<T>();
        storage.insert(entity, std::move(component));

        auto signature = entityRegistry_.signatureOf(entity);
        signature.set(id);
        entityRegistry_.setSignature(entity, signature);
    }

    template <typename T>
    void removeComponent(Entity entity) {
        const ComponentType id = componentRegistry_.componentType<T>();

        auto& storage = componentRegistry_.storage<T>();
        storage.remove(entity);

        auto signature = entityRegistry_.signatureOf(entity);
        signature.reset(id);
        entityRegistry_.setSignature(entity, signature);
    }

    template <typename T>
    [[nodiscard]] bool containsComponent(Entity entity) {
        // Must be explicitly registered; throws when called with an unregistered type.
        (void)componentRegistry_.componentType<T>();

        auto& storage = componentRegistry_.storage<T>();
        return storage.contains(entity);
    }

    template <typename T>
    [[nodiscard]] T& getComponent(Entity entity) {
        (void)componentRegistry_.componentType<T>();
        return componentRegistry_.storage<T>().get(entity);
    }

    [[nodiscard]] bool isAlive(Entity entity) const noexcept {
        return entityRegistry_.isAlive(entity);
    }

    [[nodiscard]] Signature signatureOf(Entity entity) const {
        return entityRegistry_.signatureOf(entity);
    }

private:
    EntityRegistry entityRegistry_;
    ComponentRegistry componentRegistry_;
    std::size_t maxEntityCount_{4096};
};

}  // namespace safecrowd::engine

