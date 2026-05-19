#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
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

        clearSparseIndex(entity, removedIndex);

        if (removedIndex != lastIndex) {
            components_[removedIndex] = std::move(components_[lastIndex]);
            entities_[removedIndex] = lastEntity;
            updateSparseIndex(lastEntity, removedIndex);
        }

        components_.pop_back();
        entities_.pop_back();
    }

    [[nodiscard]] bool contains(Entity entity) const noexcept {
        if (!entity.isValid()) {
            return false;
        }

        if (const auto sparseIndex = sparseIndexOf(entity);
            sparseIndex != kSparseIndexSentinel) {
            return true;
        }

        if (!shouldScanDense(entity)) {
            return false;
        }

        return findDenseIndex(entity) != kSparseIndexSentinel;
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

    [[nodiscard]] const std::vector<Entity>& entities() const noexcept {
        return entities_;
    }

private:
    static constexpr std::size_t kSparseIndexSentinel = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t kMaxSparseSlots = 1U << 20U;

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
            ensureSparseSlot(entity);
            updateSparseIndex(entity, index);
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

        std::size_t denseIndex = sparseIndexOf(entity);
        if (denseIndex == kSparseIndexSentinel && shouldScanDense(entity)) {
            denseIndex = findDenseIndex(entity);
        }

        if (denseIndex == kSparseIndexSentinel) {
            throw std::invalid_argument("Component not found for entity.");
        }

        return denseIndex;
    }

    [[nodiscard]] bool shouldScanDense(Entity entity) const noexcept {
        const auto index = static_cast<std::size_t>(entity.index);
        if (index >= kMaxSparseSlots) {
            return true;
        }

        if (index >= sparseIndices_.size()) {
            return false;
        }

        return sparseIndices_[index] != kSparseIndexSentinel;
    }

    void ensureSparseSlot(Entity entity) {
        const auto index = static_cast<std::size_t>(entity.index);
        if (index >= kMaxSparseSlots || index < sparseIndices_.size()) {
            return;
        }

        sparseIndices_.resize(index + 1, kSparseIndexSentinel);
    }

    void updateSparseIndex(Entity entity, std::size_t denseIndex) noexcept {
        const auto index = static_cast<std::size_t>(entity.index);
        if (index < sparseIndices_.size()) {
            sparseIndices_[index] = denseIndex;
        }
    }

    void clearSparseIndex(Entity entity, std::size_t denseIndex) noexcept {
        const auto index = static_cast<std::size_t>(entity.index);
        if (index >= sparseIndices_.size() || sparseIndices_[index] != denseIndex ||
            denseIndex >= entities_.size() || !(entities_[denseIndex] == entity)) {
            return;
        }

        sparseIndices_[index] = kSparseIndexSentinel;
        for (std::size_t candidate = 0; candidate < entities_.size(); ++candidate) {
            if (candidate != denseIndex && entities_[candidate].index == entity.index) {
                sparseIndices_[index] = candidate;
                return;
            }
        }
    }

    [[nodiscard]] std::size_t sparseIndexOf(Entity entity) const noexcept {
        const auto index = static_cast<std::size_t>(entity.index);
        if (index >= sparseIndices_.size()) {
            return kSparseIndexSentinel;
        }

        const std::size_t denseIndex = sparseIndices_[index];
        if (denseIndex == kSparseIndexSentinel || denseIndex >= entities_.size() ||
            !(entities_[denseIndex] == entity)) {
            return kSparseIndexSentinel;
        }

        return denseIndex;
    }

    [[nodiscard]] std::size_t findDenseIndex(Entity entity) const noexcept {
        for (std::size_t index = 0; index < entities_.size(); ++index) {
            if (entities_[index] == entity) {
                return index;
            }
        }
        return kSparseIndexSentinel;
    }

    std::vector<T> components_;
    std::vector<Entity> entities_;
    std::vector<std::size_t> sparseIndices_;
};

}  // namespace safecrowd::engine
