#include <string>

#include "TestSupport.h"

#include "domain/DemoFixtureService.h"
#include "domain/ImportIssue.h"
#include "domain/ImportValidationService.h"

SC_TEST(DemoFixtureServiceBuildsSprint1Fixture) {
    safecrowd::domain::DemoFixtureService service;
    const auto fixture = service.createSprint1DemoFixture();
    const auto& layout = fixture.layout;
    const auto& population = fixture.population;

    SC_EXPECT_EQ(layout.id, std::string("demo-fixture-01"));
    SC_EXPECT_EQ(layout.name, std::string("Sprint 1 Demo Layout"));
    SC_EXPECT_EQ(layout.levelId, std::string("L1"));
    SC_EXPECT_EQ(layout.zones.size(), std::size_t{2});
    SC_EXPECT_EQ(layout.zones.front().kind, safecrowd::domain::ZoneKind::Room);
    SC_EXPECT_EQ(layout.zones.back().kind, safecrowd::domain::ZoneKind::Exit);

    SC_EXPECT_EQ(layout.connections.size(), std::size_t{1});
    SC_EXPECT_EQ(layout.connections.front().kind, safecrowd::domain::ConnectionKind::Exit);
    SC_EXPECT_NEAR(layout.connections.front().effectiveWidth, 2.0, 1e-9);
    SC_EXPECT_EQ(layout.barriers.size(), std::size_t{1});
    SC_EXPECT_TRUE(layout.barriers.front().geometry.closed);

    SC_EXPECT_EQ(population.initialPlacements.size(), std::size_t{1});
    SC_EXPECT_EQ(population.initialPlacements.front().zoneId, std::string("zone-room-1"));
    SC_EXPECT_EQ(population.initialPlacements.front().targetAgentCount, std::size_t{100});
    SC_EXPECT_EQ(population.initialPlacements.front().area.outline.size(), std::size_t{4});

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(issues));
}
