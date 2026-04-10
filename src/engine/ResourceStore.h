#pragma once

#include <any>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace safecrowd::engine {

class ResourceStore {
public:
    template <typename T>
    void set(T resource) {
        resources_[std::type_index(typeid(T))] = std::move(resource);
    }

    template <typename T>
    [[nodiscard]] bool contains() const {
        return resources_.contains(std::type_index(typeid(T)));
    }

    template <typename T>
    [[nodiscard]] T& get() {
        return getInternal<T>();
    }

    template <typename T>
    [[nodiscard]] const T& get() const {
        return getInternal<T>();
    }

private:
    template <typename T>
    [[nodiscard]] T& getInternal() const {
        const auto it = resources_.find(std::type_index(typeid(T)));
        if (it == resources_.end()) {
            throw std::runtime_error("Requested resource is not present in ResourceStore");
        }

        auto* value = std::any_cast<T>(&it->second);
        if (value == nullptr) {
            throw std::runtime_error("Stored resource type does not match requested type");
        }

        return *value;
    }

    mutable std::unordered_map<std::type_index, std::any> resources_;
};

class WorldResources {
public:
    explicit WorldResources(ResourceStore& store)
        : store_(store) {}

    template <typename T>
    void set(T resource) {
        store_.set(std::move(resource));
    }

    template <typename T>
    [[nodiscard]] bool contains() const {
        return store_.contains<T>();
    }

    template <typename T>
    [[nodiscard]] T& get() {
        return store_.get<T>();
    }

    template <typename T>
    [[nodiscard]] const T& get() const {
        return store_.get<T>();
    }

private:
    ResourceStore& store_;
};

}  // namespace safecrowd::engine
