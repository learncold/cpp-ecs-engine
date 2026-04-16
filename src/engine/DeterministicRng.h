#pragma once

#include <cstdint>

namespace safecrowd::engine {

class DeterministicRng {
public:
    explicit DeterministicRng(std::uint64_t seed = 1) noexcept {
        reseed(seed);
    }

    void reseed(std::uint64_t seed) noexcept {
        baseSeed_ = seed == 0 ? 1 : seed;
        state_ = mix(baseSeed_);
    }

    [[nodiscard]] std::uint64_t baseSeed() const noexcept {
        return baseSeed_;
    }

    [[nodiscard]] std::uint64_t derive(std::uint64_t runIndex,
                                       std::uint64_t fixedStepIndex) const noexcept {
        auto state = mix(baseSeed_);
        state = mix(state ^ mix(runIndex));
        state = mix(state ^ mix(fixedStepIndex));
        return state;
    }

    [[nodiscard]] std::uint64_t next() noexcept {
        state_ = mix(state_);
        return state_;
    }

private:
    static std::uint64_t mix(std::uint64_t value) noexcept {
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    std::uint64_t baseSeed_{1};
    std::uint64_t state_{0};
};

}  // namespace safecrowd::engine
