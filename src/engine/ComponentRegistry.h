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

// ComponentRegistry
//
// 컴포넌트 타입(C++ 타입)을 고유 ID(ComponentType)에 매핑하고,
// 타입별로 PackedComponentStorage<T> 인스턴스를 하나씩 보관한다.
//
// - 타입은 처음 addComponent 시 자동 등록된다(getOrRegister<T>).
// - entity가 삭제될 때 notifyEntityDestroyed()를 호출하면
//   등록된 모든 storage에서 해당 entity 데이터를 일괄 제거한다(cleanup flow).
class ComponentRegistry {
public:
    // T를 레지스트리에 등록한다.
    // 이미 등록된 경우 기존 ID를 그대로 반환한다.
    // 처음 등록이면 PackedComponentStorage<T>를 함께 생성한다.
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

    // T의 ComponentType ID를 반환한다.
    // 등록되지 않은 타입이면 std::nullopt를 반환한다(예외 없음).
    template <typename T>
    [[nodiscard]] std::optional<ComponentType> tryTypeOf() const noexcept {
        const auto it = typeIds_.find(typeid(T));
        if (it == typeIds_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // T가 레지스트리에 등록되어 있는지 확인한다.
    template <typename T>
    [[nodiscard]] bool isRegistered() const noexcept {
        return typeIds_.contains(typeid(T));
    }

    // T의 PackedComponentStorage 참조를 반환한다.
    // T가 등록되지 않은 경우 예외를 던진다.
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

    // EcsCore cleanup flow 진입점.
    // entity가 destroyEntity()될 때 호출되며,
    // 등록된 모든 storage에 entityDestroyed()를 전달해
    // 해당 entity의 컴포넌트 데이터를 일괄 제거한다.
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
