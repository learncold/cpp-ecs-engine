#include <algorithm>
#include <cmath>
#include <string>

#include "TestSupport.h"

#include "domain/DemoLayouts.h"
#include "domain/DemoFixtureService.h"
#include "domain/ImportIssue.h"
#include "domain/ImportValidationService.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioSimulationRunner.h"

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

const safecrowd::domain::Connection2D* findConnectionId(
    const std::vector<safecrowd::domain::Connection2D>& connections,
    const std::string& id) {
    const auto it = std::find_if(connections.begin(), connections.end(), [&](const auto& connection) {
        return connection.id == id;
    });
    return it == connections.end() ? nullptr : &(*it);
}

bool containsBarrierId(
    const std::vector<safecrowd::domain::Barrier2D>& barriers,
    const std::string& id) {
    return std::any_of(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        return barrier.id == id;
    });
}

bool containsBlockingIssue(
    const std::vector<safecrowd::domain::ImportIssue>& issues,
    safecrowd::domain::ImportIssueCode code,
    const std::string& sourceId) {
    return std::any_of(issues.begin(), issues.end(), [&](const auto& issue) {
        return issue.code == code && issue.sourceId == sourceId && issue.blocksSimulation();
    });
}

double spanLength(const safecrowd::domain::LineSegment2D& span) {
    const auto dx = span.end.x - span.start.x;
    const auto dy = span.end.y - span.start.y;
    return std::sqrt(dx * dx + dy * dy);
}

double distanceToSegment(
    const safecrowd::domain::Point2D& point,
    const safecrowd::domain::LineSegment2D& segment) {
    const auto dx = segment.end.x - segment.start.x;
    const auto dy = segment.end.y - segment.start.y;
    const auto lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1e-12) {
        const auto px = point.x - segment.start.x;
        const auto py = point.y - segment.start.y;
        return std::sqrt(px * px + py * py);
    }

    const auto t = std::clamp(
        ((point.x - segment.start.x) * dx + (point.y - segment.start.y) * dy) / lengthSquared,
        0.0,
        1.0);
    const auto closestX = segment.start.x + dx * t;
    const auto closestY = segment.start.y + dy * t;
    const auto px = point.x - closestX;
    const auto py = point.y - closestY;
    return std::sqrt(px * px + py * py);
}

bool pointInRect(
    const safecrowd::domain::Point2D& point,
    double minX,
    double minY,
    double maxX,
    double maxY) {
    return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY;
}

void translatePolygon(safecrowd::domain::Polygon2D& polygon, double dx, double dy) {
    auto translateRing = [&](std::vector<safecrowd::domain::Point2D>& ring) {
        for (auto& point : ring) {
            point.x += dx;
            point.y += dy;
        }
    };

    translateRing(polygon.outline);
    for (auto& hole : polygon.holes) {
        translateRing(hole);
    }
}

bool containsDiffKey(
    const safecrowd::domain::ScenarioDraft& scenario,
    const std::string& key) {
    return std::any_of(scenario.variationDiffKeys.begin(), scenario.variationDiffKeys.end(), [&](const auto& diffKey) {
        return diffKey == key;
    });
}

std::size_t evacuatedCountForExit(
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    const std::string& exitZoneId) {
    const auto it = std::find_if(artifacts.exitUsage.begin(), artifacts.exitUsage.end(), [&](const auto& exitUsage) {
        return exitUsage.exitZoneId == exitZoneId;
    });
    return it == artifacts.exitUsage.end() ? std::size_t{0} : it->evacuatedCount;
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
    SC_EXPECT_EQ(fixture.baselineScenario.scenarioId, std::string("scenario-1"));
    SC_EXPECT_EQ(fixture.baselineScenario.name, std::string("Sprint 1 baseline"));
    SC_EXPECT_EQ(fixture.baselineScenario.role, safecrowd::domain::ScenarioRole::Baseline);
    SC_EXPECT_EQ(fixture.baselineScenario.population.initialPlacements.size(), std::size_t{1});
    SC_EXPECT_EQ(fixture.baselineScenario.population.initialPlacements.front().targetAgentCount, std::size_t{100});
    SC_EXPECT_EQ(fixture.baselineScenario.execution.timeLimitSeconds, 600.0);

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(issues));
}

SC_TEST(DemoFixtureServiceBuildsTwoFloorEvacuationFixture) {
    safecrowd::domain::DemoFixtureService service;
    const auto fixture = service.createTwoFloorEvacuationDemoFixture();
    const auto& layout = fixture.layout;
    const auto& population = fixture.population;
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    SC_EXPECT_EQ(layout.id, std::string(Ids::LayoutId));
    SC_EXPECT_EQ(layout.name, std::string("Two-floor Evacuation Demo Layout"));
    SC_EXPECT_EQ(layout.levelId, std::string(Ids::LowerFloorId));
    SC_EXPECT_EQ(layout.floors.size(), std::size_t{2});
    SC_EXPECT_EQ(layout.floors.at(0).id, std::string(Ids::LowerFloorId));
    SC_EXPECT_EQ(layout.floors.at(1).id, std::string(Ids::UpperFloorId));
    SC_EXPECT_EQ(layout.zones.size(), std::size_t{13});
    SC_EXPECT_EQ(layout.connections.size(), std::size_t{13});
    for (const auto& connection : layout.connections) {
        SC_EXPECT_NEAR(connection.effectiveWidth, spanLength(connection.centerSpan), 1e-9);
    }
    SC_EXPECT_TRUE(containsZoneId(layout.zones, Ids::UpperWestTrainingZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, Ids::UpperCorridorZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, Ids::LowerLobbyZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, Ids::WestExitZoneId));
    SC_EXPECT_TRUE(containsZoneId(layout.zones, Ids::EastExitZoneId));
    SC_EXPECT_TRUE(containsConnectionId(layout.connections, Ids::WestStairVerticalConnectionId));
    SC_EXPECT_TRUE(containsConnectionId(layout.connections, Ids::EastStairVerticalConnectionId));
    SC_EXPECT_TRUE(containsConnectionKind(layout.connections, safecrowd::domain::ConnectionKind::Stair));
    const auto* westStair = findConnectionId(layout.connections, Ids::WestStairVerticalConnectionId);
    SC_EXPECT_TRUE(westStair != nullptr);
    if (westStair != nullptr) {
        SC_EXPECT_EQ(westStair->lowerEntryDirection, safecrowd::domain::StairEntryDirection::South);
        SC_EXPECT_EQ(westStair->upperEntryDirection, safecrowd::domain::StairEntryDirection::South);
        SC_EXPECT_NEAR(westStair->centerSpan.start.x, 3.0, 1e-9);
        SC_EXPECT_NEAR(westStair->centerSpan.end.x, 3.0, 1e-9);
    }
    const auto* eastStair = findConnectionId(layout.connections, Ids::EastStairVerticalConnectionId);
    SC_EXPECT_TRUE(eastStair != nullptr);
    if (eastStair != nullptr) {
        SC_EXPECT_EQ(eastStair->lowerEntryDirection, safecrowd::domain::StairEntryDirection::South);
        SC_EXPECT_EQ(eastStair->upperEntryDirection, safecrowd::domain::StairEntryDirection::South);
        SC_EXPECT_NEAR(eastStair->centerSpan.start.x, 25.0, 1e-9);
        SC_EXPECT_NEAR(eastStair->centerSpan.end.x, 25.0, 1e-9);
    }
    SC_EXPECT_TRUE(std::all_of(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.floorId == "L1" || zone.floorId == "L2";
    }));

    SC_EXPECT_EQ(population.initialPlacements.size(), std::size_t{3});
    SC_EXPECT_EQ(population.initialPlacements.front().zoneId, std::string(Ids::UpperWestTrainingZoneId));
    SC_EXPECT_EQ(population.initialPlacements.front().floorId, std::string(Ids::UpperFloorId));
    SC_EXPECT_EQ(population.initialPlacements.front().targetAgentCount, std::size_t{30});
    SC_EXPECT_EQ(population.initialPlacements.at(1).targetAgentCount, std::size_t{20});
    SC_EXPECT_EQ(population.initialPlacements.at(2).targetAgentCount, std::size_t{30});
    SC_EXPECT_EQ(fixture.baselineScenario.role, safecrowd::domain::ScenarioRole::Baseline);
    SC_EXPECT_EQ(fixture.baselineScenario.population.initialPlacements.size(), std::size_t{3});
    SC_EXPECT_EQ(fixture.alternativeScenario.role, safecrowd::domain::ScenarioRole::Alternative);
    SC_EXPECT_EQ(fixture.alternativeScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(
        fixture.alternativeScenario.control.routeGuidances.front().guidedExitZoneId,
        std::string(Ids::EastExitZoneId));
    SC_EXPECT_TRUE(fixture.alternativeScenario.control.routeGuidances.front().installConnectionId.empty());
    SC_EXPECT_TRUE(containsDiffKey(fixture.alternativeScenario, "control.routeGuidances"));

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(issues));
}

SC_TEST(TwoFloorEvacuationDemoFixtureRunsAcrossFloors) {
    safecrowd::domain::DemoFixtureService service;
    auto fixture = service.createTwoFloorEvacuationDemoFixture();
    auto scenario = fixture.baselineScenario;

    auto placement = scenario.population.initialPlacements.front();
    placement.targetAgentCount = 1;
    placement.area.outline = {{4.0, 3.0}};
    scenario.population.initialPlacements = {placement};
    scenario.execution.timeLimitSeconds = 60.0;

    safecrowd::domain::ScenarioSimulationRunner runner(fixture.layout, scenario);
    for (int step = 0; step < 600 && !runner.complete(); ++step) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{1});
}

SC_TEST(TwoFloorEvacuationDemoCrowdCompletesAfterStairDescent) {
    safecrowd::domain::DemoFixtureService service;
    const auto fixture = service.createTwoFloorEvacuationDemoFixture();

    safecrowd::domain::ScenarioSimulationRunner runner(fixture.layout, fixture.baselineScenario);
    for (int step = 0; step < 6000 && !runner.complete(); ++step) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().totalAgentCount, std::size_t{80});
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{80});
}

SC_TEST(TwoFloorEvacuationDemoAlternativeCrowdCompletesAfterGuidedStairDescent) {
    safecrowd::domain::DemoFixtureService service;
    const auto fixture = service.createTwoFloorEvacuationDemoFixture();
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    safecrowd::domain::ScenarioSimulationRunner baselineRunner(fixture.layout, fixture.baselineScenario);
    for (int step = 0; step < 6000 && !baselineRunner.complete(); ++step) {
        baselineRunner.step(0.1);
    }

    safecrowd::domain::ScenarioSimulationRunner alternativeRunner(fixture.layout, fixture.alternativeScenario);
    for (int step = 0; step < 6000 && !alternativeRunner.complete(); ++step) {
        alternativeRunner.step(0.1);
    }

    SC_EXPECT_EQ(baselineRunner.frame().totalAgentCount, std::size_t{80});
    SC_EXPECT_EQ(baselineRunner.frame().evacuatedAgentCount, std::size_t{80});
    SC_EXPECT_EQ(alternativeRunner.frame().totalAgentCount, std::size_t{80});
    SC_EXPECT_EQ(alternativeRunner.frame().evacuatedAgentCount, std::size_t{80});
    SC_EXPECT_TRUE(alternativeRunner.frame().agents.empty());

    const auto baselineEastExitCount = evacuatedCountForExit(
        baselineRunner.resultArtifacts(),
        Ids::EastExitZoneId);
    const auto alternativeEastExitCount = evacuatedCountForExit(
        alternativeRunner.resultArtifacts(),
        Ids::EastExitZoneId);
    SC_EXPECT_TRUE(alternativeEastExitCount > baselineEastExitCount);
    SC_EXPECT_TRUE(alternativeEastExitCount > std::size_t{0});
}

SC_TEST(TwoFloorEvacuationDemoCrowdMovesOffLowerStairPortalAfterLanding) {
    safecrowd::domain::DemoFixtureService service;
    auto fixture = service.createTwoFloorEvacuationDemoFixture();
    auto scenario = fixture.baselineScenario;
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "upper-west-stair-crowd";
    placement.floorId = Ids::UpperFloorId;
    placement.zoneId = Ids::UpperWestStairZoneId;
    placement.initialVelocity = {.x = 1.2, .y = 0.0};
    placement.explicitPositions = {
        {.x = 4.45, .y = 12.45},
        {.x = 4.05, .y = 12.45},
        {.x = 4.45, .y = 12.05},
        {.x = 4.05, .y = 12.05},
        {.x = 4.45, .y = 11.65},
        {.x = 4.05, .y = 11.65},
    };
    scenario.population.initialPlacements = {placement};
    scenario.execution.timeLimitSeconds = 30.0;

    safecrowd::domain::ScenarioSimulationRunner runner(fixture.layout, scenario);
    const safecrowd::domain::LineSegment2D lowerWestPortal{{3.0, 11.6}, {3.0, 12.8}};
    bool observedLowerFloor = false;
    bool observedMovementOffPortal = false;
    for (int step = 0; step < 300 && !runner.complete(); ++step) {
        runner.step(0.1);
        for (const auto& agent : runner.frame().agents) {
            if (agent.floorId != Ids::LowerFloorId) {
                continue;
            }
            observedLowerFloor = true;
            if (distanceToSegment(agent.position, lowerWestPortal) > 0.45 || agent.position.y < 11.2) {
                observedMovementOffPortal = true;
            }
        }
    }

    std::size_t stalledLowerStairAgents = 0;
    for (const auto& agent : runner.frame().agents) {
        if (agent.floorId == Ids::LowerFloorId
            && pointInRect(agent.position, 1.0, 9.0, 3.0, 13.0)
            && agent.stalled) {
            ++stalledLowerStairAgents;
        }
    }

    SC_EXPECT_TRUE(observedLowerFloor);
    SC_EXPECT_TRUE(observedMovementOffPortal || runner.frame().agents.empty());
    SC_EXPECT_EQ(stalledLowerStairAgents, std::size_t{0});
}

SC_TEST(DemoFixtureBlockedDoorResultFixturePreservesScenarioAndResultData) {
    safecrowd::domain::DemoFixtureService service;
    const auto fixture = service.createSprint1BlockedDoorResultFixture();
    const auto& replayFrames = fixture.artifacts.replayFrames;

    SC_EXPECT_EQ(fixture.baselineScenario.role, safecrowd::domain::ScenarioRole::Baseline);
    SC_EXPECT_EQ(fixture.alternativeScenario.role, safecrowd::domain::ScenarioRole::Alternative);
    SC_EXPECT_EQ(fixture.alternativeScenario.control.connectionBlocks.size(), std::size_t{1});
    SC_EXPECT_EQ(
        fixture.alternativeScenario.control.connectionBlocks.front().connectionId,
        std::string(safecrowd::domain::DemoLayouts::Sprint1FacilityIds::DoorwayConnectionId));
    SC_EXPECT_TRUE(containsDiffKey(fixture.alternativeScenario, "control.connectionBlocks"));

    SC_EXPECT_TRUE(fixture.frame.complete);
    SC_EXPECT_EQ(fixture.frame.totalAgentCount, std::size_t{100});
    SC_EXPECT_EQ(fixture.frame.evacuatedAgentCount, std::size_t{100});
    SC_EXPECT_TRUE(!fixture.risk.hotspots.empty());
    SC_EXPECT_TRUE(!fixture.risk.bottlenecks.empty());
    SC_EXPECT_TRUE(!fixture.artifacts.evacuationProgress.empty());
    SC_EXPECT_TRUE(replayFrames.size() > std::size_t{1});
    for (std::size_t index = 1; index < replayFrames.size(); ++index) {
        SC_EXPECT_TRUE(replayFrames[index - 1].elapsedSeconds < replayFrames[index].elapsedSeconds);
    }
    SC_EXPECT_NEAR(replayFrames.back().elapsedSeconds, fixture.frame.elapsedSeconds, 1e-9);
    SC_EXPECT_EQ(replayFrames.back().complete, fixture.frame.complete);
    SC_EXPECT_EQ(replayFrames.back().totalAgentCount, fixture.frame.totalAgentCount);
    SC_EXPECT_EQ(replayFrames.back().evacuatedAgentCount, fixture.frame.evacuatedAgentCount);
    SC_EXPECT_TRUE(std::any_of(replayFrames.begin(), replayFrames.end() - 1, [](const auto& frame) {
        return !frame.agents.empty();
    }));
    SC_EXPECT_TRUE(fixture.artifacts.timingSummary.finalEvacuationTimeSeconds.has_value());
    SC_EXPECT_TRUE(fixture.artifacts.timingSummary.t90Frame.has_value());
    SC_EXPECT_TRUE(fixture.artifacts.timingSummary.t95Frame.has_value());
    SC_EXPECT_NEAR(
        fixture.artifacts.timingSummary.t90Frame->elapsedSeconds,
        *fixture.artifacts.timingSummary.t90Seconds,
        1e-9);
    SC_EXPECT_NEAR(
        fixture.artifacts.timingSummary.t95Frame->elapsedSeconds,
        *fixture.artifacts.timingSummary.t95Seconds,
        1e-9);
    SC_EXPECT_TRUE(
        std::any_of(fixture.risk.hotspots.begin(), fixture.risk.hotspots.end(), [](const auto& hotspot) {
            return hotspot.detectionFrame.has_value();
        })
        || std::any_of(fixture.risk.bottlenecks.begin(), fixture.risk.bottlenecks.end(), [](const auto& bottleneck) {
            return bottleneck.detectionFrame.has_value();
        }));
    SC_EXPECT_TRUE(
        !fixture.artifacts.densitySummary.peakCells.empty()
        || !fixture.artifacts.densitySummary.peakField.cells.empty());
    SC_EXPECT_TRUE(!fixture.artifacts.exitUsage.empty());
    SC_EXPECT_TRUE(!fixture.artifacts.zoneCompletion.empty());
    SC_EXPECT_TRUE(!fixture.artifacts.placementCompletion.empty());
}

SC_TEST(DemoLayoutRejectsMovedConnectionSpan) {
    auto layout = safecrowd::domain::DemoLayouts::demoFacility();
    auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [](const auto& connection) {
        return connection.id == safecrowd::domain::DemoLayouts::Sprint1FacilityIds::OpeningConnectionId;
    });
    SC_EXPECT_TRUE(it != layout.connections.end());

    it->centerSpan.start.x += 1.0;
    it->centerSpan.end.x += 1.0;

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(containsBlockingIssue(
        issues,
        safecrowd::domain::ImportIssueCode::ConnectionSpanMisaligned,
        safecrowd::domain::DemoLayouts::Sprint1FacilityIds::OpeningConnectionId));
}

SC_TEST(DemoLayoutRejectsMovedExitZone) {
    auto layout = safecrowd::domain::DemoLayouts::demoFacility();
    auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.id == safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitZoneId;
    });
    SC_EXPECT_TRUE(it != layout.zones.end());

    translatePolygon(it->area, 2.0, 0.0);

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(containsBlockingIssue(
        issues,
        safecrowd::domain::ImportIssueCode::ConnectionSpanMisaligned,
        safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitConnectionId));
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
