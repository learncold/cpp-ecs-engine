#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <ostream>

namespace safecrowd::engine {

using EntityIndex = std::uint32_t;
using EntityGeneration = std::uint32_t;

struct Entity {
    static constexpr EntityIndex invalidIndex = std::numeric_limits<EntityIndex>::max();

    EntityIndex index{invalidIndex};
    EntityGeneration generation{0};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return index != invalidIndex;
    }

    [[nodiscard]] static constexpr Entity invalid() noexcept {
        return {};
    }

    auto operator<=>(const Entity&) const = default;
};

inline std::ostream& operator<<(std::ostream& stream, const Entity& entity) {
    stream << "Entity{index=" << entity.index << ", generation=" << entity.generation << "}";
    return stream;
}

}  // namespace safecrowd::engine