#pragma once

#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "engine/EcsCore.h"

namespace safecrowd::engine {

class CommandBuffer {
public:
    template <typename... Ts>
    void spawnEntity(Ts... components) {
        commands_.push_back(
            [comps = std::make_tuple(std::move(components)...)](EcsCore& core) mutable {
                Entity e = core.createEntity();
                std::apply([&](auto&... c) { (core.addComponent(e, std::move(c)), ...); }, comps);
            });
    }

    void destroyEntity(Entity entity) {
        commands_.emplace_back([entity](EcsCore& core) {
            core.destroyEntity(entity);
        });
    }

    template <typename T>
    void addComponent(Entity entity, T component) {
        commands_.emplace_back([entity, comp = std::move(component)](EcsCore& core) mutable {
            core.addComponent(entity, std::move(comp));
        });
    }

    template <typename T>
    void removeComponent(Entity entity) {
        commands_.emplace_back([entity](EcsCore& core) {
            core.removeComponent<T>(entity);
        });
    }

    void flush(EcsCore& core) {
        for (auto& cmd : commands_) {
            cmd(core);
        }
        commands_.clear();
    }

    [[nodiscard]] bool empty() const noexcept {
        return commands_.empty();
    }

private:
    std::vector<std::function<void(EcsCore&)>> commands_;
};

class WorldCommands {
public:
    explicit WorldCommands(CommandBuffer& buffer) : buffer_(buffer) {}

    template <typename... Ts>
    void spawnEntity(Ts... components) {
        buffer_.spawnEntity(std::move(components)...);
    }

    void destroyEntity(Entity entity) {
        buffer_.destroyEntity(entity);
    }

    template <typename T>
    void addComponent(Entity entity, T component) {
        buffer_.addComponent(entity, std::move(component));
    }

    template <typename T>
    void removeComponent(Entity entity) {
        buffer_.removeComponent<T>(entity);
    }

private:
    CommandBuffer& buffer_;
};

}  // namespace safecrowd::engine
