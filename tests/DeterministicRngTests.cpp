#include "TestSupport.h"

#include "engine/DeterministicRng.h"

SC_TEST(DeterministicRng_Derive_IsStableForSameInputs) {
    safecrowd::engine::DeterministicRng rng{17};

    const auto first = rng.derive(2, 5);
    const auto second = rng.derive(2, 5);
    const auto differentRun = rng.derive(3, 5);
    const auto differentStep = rng.derive(2, 6);

    SC_EXPECT_EQ(first, second);
    SC_EXPECT_TRUE(first != differentRun);
    SC_EXPECT_TRUE(first != differentStep);
}

SC_TEST(DeterministicRng_Reseed_RestartsSequence) {
    safecrowd::engine::DeterministicRng rng{23};

    const auto first = rng.next();
    const auto second = rng.next();

    rng.reseed(23);

    SC_EXPECT_EQ(rng.next(), first);
    SC_EXPECT_EQ(rng.next(), second);
}
