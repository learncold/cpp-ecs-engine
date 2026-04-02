#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "engine/Entity.h"
#include "engine/EntityRegistry.h"
#include "engine/IComponentStorage.h"
#include "engine/PackedComponentStorage.h"

namespace safecrowd::engine {

using ComponentType = std::size_t;

class ComponentRegistry {
public:
    template <typename T>
    void registerType() {
        const std::type_index key{typeid(T)};

        if (const auto it = typeToId_.find(key); it != typeToId_.end()) {
            return;  // idempotent
        }

        if (nextId_ >= kMaxComponentTypes) {
            throw std::overflow_error("Component type limit exceeded.");
        }

        const ComponentType id = nextId_++;
        typeToId_.emplace(key, id);

        if (storages_.size() < (id + 1U)) {
            storages_.resize(id + 1U);
        }

        storages_[id] = std::make_unique<PackedComponentStorage<T>>();
    }

    template <typename T>
    [[nodiscard]] ComponentType componentType() const {
        const std::type_index key{typeid(T)};
        const auto it = typeToId_.find(key);
        if (it == typeToId_.end()) {
            throw std::logic_error("Component type not registered. Call registerType<T>() during initialization.");
        }
        return it->second;
    }

    template <typename T>
    [[nodiscard]] PackedComponentStorage<T>& storage() {
        const ComponentType id = componentType<T>();

        if (id >= storages_.size() || storages_[id] == nullptr) {
            // Storage may be cleared during shutdown; recreate lazily while type ids remain valid.
            if (id >= storages_.size()) {
                storages_.resize(id + 1U);
            }
            storages_[id] = std::make_unique<PackedComponentStorage<T>>();
        }

        return *static_cast<PackedComponentStorage<T>*>(storages_[id].get());
    }

    template <typename T>
    [[nodiscard]] const PackedComponentStorage<T>& storage() const {
        const ComponentType id = componentType<T>();

        if (id >= storages_.size() || storages_[id] == nullptr) {
            // Lazily creating storages inside a const method is not possible without breaking const-correctness.
            // For const access, assume shutdown clears storages by resetting the pointers but keeps them recreated on non-const access.
            // Call the non-const storage() when you need access across shutdown.
            throw std::logic_error("Component storage is not available (did it get cleared by shutdown?).");
        }

        return *static_cast<const PackedComponentStorage<T>*>(storages_[id].get());
    }

    void entityDestroyed(Entity entity) {
        for (auto& storage : storages_) {
            if (storage != nullptr) {
                storage->entityDestroyed(entity);
            }
        }
    }

    // Clears stored component data to release memory, while preserving type IDs.
    void shutdown() noexcept {
        for (auto& storage : storages_) {
            storage.reset();
        }
    }

private:
    std::unordered_map<std::type_index, ComponentType> typeToId_;
    ComponentType nextId_{0};
    std::vector<std::unique_ptr<IComponentStorage>> storages_;
};

}  // namespace safecrowd::engine

