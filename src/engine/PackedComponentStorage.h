#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/IComponentStorage.h"

namespace safecrowd::engine {

template <typename T>
class PackedComponentStorage final : public IComponentStorage {
public:
    void insert(Entity entity, const T& component) {
        insertImpl(entity, component);
    }

    void insert(Entity entity, T&& component) {
        insertImpl(entity, std::move(component));
    }

    void remove(Entity entity) {
        const std::size_t removedIndex = indexOf(entity);
        const std::size_t lastIndex = components_.size() - 1;
        const Entity lastEntity = entities_[lastIndex];

        if (removedIndex != lastIndex) {
            components_[removedIndex] = std::move(components_[lastIndex]);
            entities_[removedIndex] = lastEntity;
            entityToIndex_[lastEntity] = removedIndex;
        }

        components_.pop_back();
        entities_.pop_back();
        entityToIndex_.erase(entity);
    }

    [[nodiscard]] bool contains(Entity entity) const noexcept {
        return entity.isValid() && entityToIndex_.contains(entity);
    }

    [[nodiscard]] T& get(Entity entity) {
        return components_[indexOf(entity)];
    }

    [[nodiscard]] const T& get(Entity entity) const {
        return components_[indexOf(entity)];
    }

    void entityDestroyed(Entity entity) override {
        if (contains(entity)) {
            remove(entity);
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return components_.size();
    }

private:
    struct EntityHash {
        [[nodiscard]] std::size_t operator()(const Entity& entity) const noexcept {
            const auto packed =
                (static_cast<std::uint64_t>(entity.generation) << 32U) | entity.index;
            return std::hash<std::uint64_t>{}(packed);
        }
    };

    template <typename U>
    void insertImpl(Entity entity, U&& component) {
        if (!entity.isValid()) {
            throw std::invalid_argument("Invalid entity handle.");
        }

        if (contains(entity)) {
            throw std::invalid_argument("Component already exists for entity.");
        }

        const std::size_t index = components_.size();
        components_.push_back(std::forward<U>(component));

        try {
            entities_.push_back(entity);
        } catch (...) {
            components_.pop_back();
            throw;
        }

        try {
            entityToIndex_.emplace(entity, index);
        } catch (...) {
            entities_.pop_back();
            components_.pop_back();
            throw;
        }
    }

    [[nodiscard]] std::size_t indexOf(Entity entity) const {
        if (!entity.isValid()) {
            throw std::invalid_argument("Invalid entity handle.");
        }

        const auto it = entityToIndex_.find(entity);
        if (it == entityToIndex_.end()) {
            throw std::invalid_argument("Component not found for entity.");
        }

        return it->second;
    }

    std::vector<T> components_;
    std::vector<Entity> entities_;
    std::unordered_map<Entity, std::size_t, EntityHash> entityToIndex_;
};

}  // namespace safecrowd::engine
