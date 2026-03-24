#include <stdexcept>

#include "TestSupport.h"

#include "engine/EntityRegistry.h"

SC_TEST(EntityRegistryReusesIndexWithNewGeneration) {
    safecrowd::engine::EntityRegistry registry(1);

    const auto first = registry.allocate();
    SC_EXPECT_TRUE(registry.isAlive(first));

    registry.release(first);
    SC_EXPECT_TRUE(!registry.isAlive(first));

    const auto second = registry.allocate();
    SC_EXPECT_EQ(second.index, first.index);
    SC_EXPECT_TRUE(second.generation > first.generation);
    SC_EXPECT_TRUE(registry.isAlive(second));
}

SC_TEST(EntityRegistryRejectsStaleEntityHandles) {
    safecrowd::engine::EntityRegistry registry(1);

    const auto entity = registry.allocate();
    registry.release(entity);

    bool threwOnRelease = false;
    try {
        registry.release(entity);
    } catch (const std::invalid_argument&) {
        threwOnRelease = true;
    }

    SC_EXPECT_TRUE(threwOnRelease);

    bool threwOnSignatureRead = false;
    try {
        static_cast<void>(registry.signatureOf(entity));
    } catch (const std::invalid_argument&) {
        threwOnSignatureRead = true;
    }

    SC_EXPECT_TRUE(threwOnSignatureRead);
}

SC_TEST(EntityRegistryStoresSignaturesPerLiveEntity) {
    safecrowd::engine::EntityRegistry registry(1);

    const auto entity = registry.allocate();
    safecrowd::engine::Signature signature;
    signature.set(1);
    signature.set(5);

    registry.setSignature(entity, signature);

    const auto stored = registry.signatureOf(entity);
    SC_EXPECT_EQ(stored, signature);
}