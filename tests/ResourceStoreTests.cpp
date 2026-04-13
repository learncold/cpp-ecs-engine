#include "TestSupport.h"

#include <stdexcept>
#include <string>

#include "engine/ResourceStore.h"

namespace {

struct SampleResource {
    int value{0};
};

}  // namespace

SC_TEST(ResourceStore_SetGetAndContainsWork) {
    safecrowd::engine::ResourceStore store;

    store.set(SampleResource{42});

    SC_EXPECT_TRUE(store.contains<SampleResource>());
    SC_EXPECT_EQ(store.get<SampleResource>().value, 42);
}

SC_TEST(ResourceStore_GetMissingResourceThrows) {
    safecrowd::engine::ResourceStore store;

    bool threw = false;
    try {
        (void)store.get<SampleResource>();
    } catch (const std::runtime_error&) {
        threw = true;
    }

    SC_EXPECT_TRUE(threw);
}

SC_TEST(WorldResources_DelegatesToUnderlyingStore) {
    safecrowd::engine::ResourceStore store;
    safecrowd::engine::WorldResources resources{store};

    resources.set(std::string{"seeded"});

    SC_EXPECT_TRUE(resources.contains<std::string>());
    SC_EXPECT_EQ(resources.get<std::string>(), std::string{"seeded"});
    SC_EXPECT_EQ(store.get<std::string>(), std::string{"seeded"});
}
