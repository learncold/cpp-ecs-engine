#include <memory>
#include <stdexcept>

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
