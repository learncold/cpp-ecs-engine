#pragma once

#include <bitset>
#include <cstddef>
#include <deque>
#include <vector>

#include "engine/Entity.h"

namespace safecrowd::engine {

inline constexpr std::size_t kMaxComponentTypes = 64;
using Signature = std::bitset<kMaxComponentTypes>;

class EntityRegistry {
public:
    explicit EntityRegistry(std::size_t maxEntityCount = 4096);

    [[nodiscard]] Entity allocate();
    void release(Entity entity);
    [[nodiscard]] bool isAlive(Entity entity) const noexcept;
    void setSignature(Entity entity, Signature signature);
    [[nodiscard]] Signature signatureOf(Entity entity) const;

private:
    struct Entry {
        EntityGeneration generation{0};
        bool alive{false};
        Signature signature{};
    };

    [[nodiscard]] const Entry& entryFor(Entity entity) const;
    [[nodiscard]] Entry& entryFor(Entity entity);

    std::vector<Entry> entries_;
    std::deque<EntityIndex> freeIndices_;
};

}  // namespace safecrowd::engine