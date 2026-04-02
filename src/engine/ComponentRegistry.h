#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

#include "engine/EntityRegistry.h"
#include "engine/IComponentStorage.h"
#include "engine/PackedComponentStorage.h"

namespace safecrowd::engine {

using ComponentType = std::size_t;

class ComponentRegistry {
public:
    template <typename T>
    ComponentType getOrRegister() {
        const std::type_index key = typeid(T);

        if (const auto it = typeIds_.find(key); it != typeIds_.end()) {
            return it->second;
        }

        if (nextTypeId_ >= kMaxComponentTypes) {
            throw std::runtime_error(
                "ComponentRegistry: 최대 컴포넌트 타입 수를 초과했습니다.");
        }

        const ComponentType id = nextTypeId_++;
        typeIds_.emplace(key, id);
        storages_.emplace(key, std::make_unique<PackedComponentStorage<T>>());

        return id;
    }

    template <typename T>
    [[nodiscard]] std::optional<ComponentType> tryTypeOf() const noexcept {
        const auto it = typeIds_.find(typeid(T));
        if (it == typeIds_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    template <typename T>
    [[nodiscard]] bool isRegistered() const noexcept {
        return typeIds_.contains(typeid(T));
    }

    template <typename T>
    [[nodiscard]] PackedComponentStorage<T>& storageFor() {
        const auto it = storages_.find(typeid(T));
        if (it == storages_.end()) {
            throw std::runtime_error(
                "ComponentRegistry: 등록되지 않은 컴포넌트 타입입니다.");
        }
        return static_cast<PackedComponentStorage<T>&>(*it->second);
    }

    template <typename T>
    [[nodiscard]] const PackedComponentStorage<T>& storageFor() const {
        const auto it = storages_.find(typeid(T));
        if (it == storages_.end()) {
            throw std::runtime_error(
                "ComponentRegistry: 등록되지 않은 컴포넌트 타입입니다.");
        }
        return static_cast<const PackedComponentStorage<T>&>(*it->second);
    }

    void notifyEntityDestroyed(Entity entity) {
        for (auto& [key, storage] : storages_) {
            storage->entityDestroyed(entity);
        }
    }

private:
    std::unordered_map<std::type_index, ComponentType> typeIds_;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages_;
    ComponentType nextTypeId_{0};
};

}  // namespace safecrowd::engine
