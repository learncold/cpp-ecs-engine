#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "TestSupport.h"

#include "engine/IComponentStorage.h"
#include "engine/PackedComponentStorage.h"

namespace {

struct TestComponent {
    int value{0};
};

}  // namespace

SC_TEST(PackedComponentStorageStoresAndRemovesComponentsDensely) {
    using safecrowd::engine::Entity;
    using safecrowd::engine::PackedComponentStorage;

    PackedComponentStorage<TestComponent> storage;

    const Entity first{1, 0};
    const Entity second{2, 0};
    const Entity third{3, 0};

    storage.insert(first, TestComponent{10});
    storage.insert(second, TestComponent{20});
    storage.insert(third, TestComponent{30});

    SC_EXPECT_TRUE(storage.contains(first));
    SC_EXPECT_TRUE(storage.contains(second));
    SC_EXPECT_TRUE(storage.contains(third));
    SC_EXPECT_EQ(storage.size(), static_cast<std::size_t>(3));
    SC_EXPECT_EQ(storage.get(second).value, 20);

    storage.remove(second);

    SC_EXPECT_TRUE(storage.contains(first));
    SC_EXPECT_TRUE(!storage.contains(second));
    SC_EXPECT_TRUE(storage.contains(third));
    SC_EXPECT_EQ(storage.size(), static_cast<std::size_t>(2));
    SC_EXPECT_EQ(storage.get(first).value, 10);
    SC_EXPECT_EQ(storage.get(third).value, 30);

    const auto& denseEntities = storage.entities();
    SC_EXPECT_EQ(denseEntities.size(), std::size_t{2});
    SC_EXPECT_TRUE(denseEntities[0] == first);
    SC_EXPECT_TRUE(denseEntities[1] == third);

    bool threwOnMissingComponent = false;
    try {
        static_cast<void>(storage.get(second));
    } catch (const std::invalid_argument&) {
        threwOnMissingComponent = true;
    }

    SC_EXPECT_TRUE(threwOnMissingComponent);
}

SC_TEST(PackedComponentStorageRejectsInvalidOrDuplicateEntities) {
    using safecrowd::engine::Entity;
    using safecrowd::engine::PackedComponentStorage;

    PackedComponentStorage<int> storage;
    const Entity entity{7, 1};

    storage.insert(entity, 42);

    bool threwOnDuplicateInsert = false;
    try {
        storage.insert(entity, 99);
    } catch (const std::invalid_argument&) {
        threwOnDuplicateInsert = true;
    }

    SC_EXPECT_TRUE(threwOnDuplicateInsert);

    bool threwOnInvalidInsert = false;
    try {
        storage.insert(Entity::invalid(), 1);
    } catch (const std::invalid_argument&) {
        threwOnInvalidInsert = true;
    }

    SC_EXPECT_TRUE(threwOnInvalidInsert);

    bool threwOnMissingRemove = false;
    try {
        storage.remove(Entity{99, 1});
    } catch (const std::invalid_argument&) {
        threwOnMissingRemove = true;
    }

    SC_EXPECT_TRUE(threwOnMissingRemove);
}

SC_TEST(PackedComponentStorageRejectsStaleGenerationLookup) {
    using safecrowd::engine::Entity;
    using safecrowd::engine::PackedComponentStorage;

    PackedComponentStorage<int> storage;
    const Entity stored{5, 2};
    const Entity stale{5, 1};

    storage.insert(stored, 42);

    SC_EXPECT_TRUE(storage.contains(stored));
    SC_EXPECT_TRUE(!storage.contains(stale));

    bool threwOnStaleGet = false;
    try {
        static_cast<void>(storage.get(stale));
    } catch (const std::invalid_argument& error) {
        threwOnStaleGet = true;
        SC_EXPECT_TRUE(std::string{error.what()} == "Component not found for entity.");
    }

    SC_EXPECT_TRUE(threwOnStaleGet);
    SC_EXPECT_EQ(storage.get(stored), 42);
}

SC_TEST(PackedComponentStorageKeepsDistinctGenerationsWithSameIndex) {
    using safecrowd::engine::Entity;
    using safecrowd::engine::PackedComponentStorage;

    PackedComponentStorage<int> storage;
    const Entity older{9, 1};
    const Entity newer{9, 2};

    storage.insert(older, 10);
    storage.insert(newer, 20);

    SC_EXPECT_TRUE(storage.contains(older));
    SC_EXPECT_TRUE(storage.contains(newer));
    SC_EXPECT_EQ(storage.get(older), 10);
    SC_EXPECT_EQ(storage.get(newer), 20);

    storage.remove(newer);

    SC_EXPECT_TRUE(storage.contains(older));
    SC_EXPECT_TRUE(!storage.contains(newer));
    SC_EXPECT_EQ(storage.get(older), 10);
}

SC_TEST(PackedComponentStorageHandlesLargeEntityIndexWithoutHugeSparseResize) {
    using safecrowd::engine::Entity;
    using safecrowd::engine::EntityIndex;
    using safecrowd::engine::PackedComponentStorage;

    PackedComponentStorage<int> storage;
    const Entity large{
        static_cast<EntityIndex>(std::numeric_limits<EntityIndex>::max() - 1U),
        3,
    };

    storage.insert(large, 99);

    SC_EXPECT_TRUE(storage.contains(large));
    SC_EXPECT_EQ(storage.get(large), 99);
    SC_EXPECT_EQ(storage.size(), std::size_t{1});

    storage.remove(large);

    SC_EXPECT_TRUE(!storage.contains(large));
    SC_EXPECT_EQ(storage.size(), std::size_t{0});
}

SC_TEST(IComponentStorageEntityDestroyedRemovesStoredComponent) {
    using safecrowd::engine::Entity;
    using safecrowd::engine::IComponentStorage;
    using safecrowd::engine::PackedComponentStorage;

    auto storage = std::make_unique<PackedComponentStorage<int>>();
    PackedComponentStorage<int>* typedStorage = storage.get();
    IComponentStorage& baseStorage = *storage;

    const Entity entity{12, 2};
    const Entity survivor{13, 2};

    typedStorage->insert(entity, 5);
    typedStorage->insert(survivor, 6);

    baseStorage.entityDestroyed(entity);
    baseStorage.entityDestroyed(Entity{111, 0});

    SC_EXPECT_TRUE(!typedStorage->contains(entity));
    SC_EXPECT_TRUE(typedStorage->contains(survivor));
    SC_EXPECT_EQ(typedStorage->get(survivor), 6);
}
