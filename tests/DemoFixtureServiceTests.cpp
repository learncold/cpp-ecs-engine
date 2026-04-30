#include <algorithm>
#include <cmath>
#include <string>

#include "TestSupport.h"

#include "domain/DemoLayouts.h"
#include "domain/DemoFixtureService.h"
#include "domain/ImportIssue.h"
#include "domain/ImportValidationService.h"

namespace {

bool containsConnectionKind(
    const std::vector<safecrowd::domain::Connection2D>& connections,
    safecrowd::domain::ConnectionKind kind) {
    return std::any_of(connections.begin(), connections.end(), [&](const auto& connection) {
        return connection.kind == kind;
    });
}

bool containsZoneId(
    const std::vector<safecrowd::domain::Zone2D>& zones,
    const std::string& id) {
    return std::any_of(zones.begin(), zones.end(), [&](const auto& zone) {
        return zone.id == id;
    });
}

bool containsConnectionId(
    const std::vector<safecrowd::domain::Connection2D>& connections,
    const std::string& id) {
    return std::any_of(connections.begin(), connections.end(), [&](const auto& connection) {
        return connection.id == id;
    });
}

bool containsBarrierId(
    const std::vector<safecrowd::domain::Barrier2D>& barriers,
    const std::string& id) {
    return std::any_of(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        return barrier.id == id;
    });
}

double spanLength(const safecrowd::domain::LineSegment2D& span) {
    const auto dx = span.end.x - span.start.x;
    const auto dy = span.end.y - span.start.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

SC_TEST(DemoFixtureServiceBuildsSprint1Fixture) {
    safecrowd::domain::DemoFixtureService service;
    const auto fixture = service.createSprint1DemoFixture();
    const auto& layout = fixture.layout;
    const auto& population = fixture.population;

    SC_EXPECT_EQ(layout.id, std::string(safecrowd::domain::DemoLayouts::Sprint1FacilityIds::LayoutId));
    SC_EXPECT_EQ(layout.name, std::string("Sprint 1 Demo Layout"));
    SC_EXPECT_EQ(layout.levelId, std::string("L1"));
    SC_EXPECT_EQ(layout.floors.size(), std::size_t{1});
    SC_EXPECT_EQ(layout.floors.front().id, std::string("L1"));
    SC_EXPECT_EQ(layout.zones.size(), std::size_t{4});
    SC_EXPECT_EQ(layout.zones.at(0).kind, safecrowd::domain::ZoneKind::Room);
    SC_EXPECT_EQ(layout.zones.at(1).kind, safecrowd::domain::ZoneKind::Room);
    SC_EXPECT_EQ(layout.zones.at(2).kind, safecrowd::domain::ZoneKind::Room);
    SC_EXPECT_EQ(layout.zones.at(3).kind, safecrowd::domain::ZoneKind::Exit);
    SC_EXPECT_TRUE(std::all_of(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.floorId == "L1";
    }));

    SC_EXPECT_EQ(layout.connections.size(), std::size_t{3});
    SC_EXPECT_TRUE(containsConnectionKind(layout.connections, safecrowd::domain::ConnectionKind::Opening));
    SC_EXPECT_TRUE(containsConnectionKind(layout.connections, safecrowd::domain::ConnectionKind::Doorway));
    SC_EXPECT_TRUE(containsConnectionKind(layout.connections, safecrowd::domain::ConnectionKind::Exit));
    SC_EXPECT_EQ(layout.barriers.size(), std::size_t{14});
    SC_EXPECT_TRUE(std::any_of(layout.barriers.begin(), layout.barriers.end(), [](const auto& barrier) {
        return barrier.geometry.closed;
    }));
    SC_EXPECT_TRUE(std::all_of(layout.barriers.begin(), layout.barriers.end(), [](const auto& barrier) {
        return barrier.geometry.closed || barrier.geometry.vertices.size() == std::size_t{2};
    }));

    SC_EXPECT_EQ(population.initialPlacements.size(), std::size_t{1});
    SC_EXPECT_EQ(population.initialPlacements.front().zoneId, std::string(safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId));
    SC_EXPECT_EQ(population.initialPlacements.front().targetAgentCount, std::size_t{100});
    SC_EXPECT_EQ(population.initialPlacements.front().area.outline.size(), std::size_t{4});

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(issues));
}

SC_TEST(DemoLayoutsProvidesRuntimeFacilityLayout) {
    const auto layout = safecrowd::domain::DemoLayouts::demoFacility();

    SC_EXPECT_EQ(layout.id, std::string(safecrowd::domain::DemoLayouts::Sprint1FacilityIds::LayoutId));
    SC_EXPECT_EQ(layout.zones.size(), std::size_t{4});
    SC_EXPECT_EQ(layout.connections.size(), std::size_t{3});
    SC_EXPECT_EQ(layout.barriers.size(), std::size_t{14});
    SC_EXPECT_TRUE(containsZoneId(layout.zones, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::SideRoomZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitPassageZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitZoneId));
    SC_EXPECT_TRUE(containsConnectionId(layout.connections, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::OpeningConnectionId));
    SC_EXPECT_TRUE(containsConnectionId(layout.connections, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::DoorwayConnectionId));
    SC_EXPECT_TRUE(containsConnectionId(layout.connections, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitConnectionId));
    SC_EXPECT_TRUE(containsBarrierId(layout.barriers, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomNorthWallId));
    SC_EXPECT_TRUE(containsBarrierId(layout.barriers, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::SideRoomNorthWallId));
    SC_EXPECT_TRUE(containsBarrierId(layout.barriers, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::PassageNorthWallId));
    for (const auto& connection : layout.connections) {
        SC_EXPECT_NEAR(connection.effectiveWidth, spanLength(connection.centerSpan), 1e-9);
    }
    for (const auto& barrier : layout.barriers) {
        SC_EXPECT_TRUE(barrier.geometry.closed || barrier.geometry.vertices.size() == std::size_t{2});
    }

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(issues));
}
