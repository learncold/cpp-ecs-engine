#include <algorithm>
#include <string>
#include <vector>

#include "TestSupport.h"

#include "domain/FacilityLayoutBuilder.h"
#include "domain/ImportValidationService.h"

namespace {

bool containsIssueCode(
    const std::vector<safecrowd::domain::ImportIssue>& issues,
    safecrowd::domain::ImportIssueCode code) {
    return std::any_of(issues.begin(), issues.end(), [&](const auto& issue) {
        return issue.code == code;
    });
}

}  // namespace

SC_TEST(FacilityLayoutBuilderBuildsWalkableExitAndBarrierElements) {
    safecrowd::domain::CanonicalGeometry geometry;
    geometry.levelId = "L1";
    geometry.walkableAreas.push_back({
        .id = "walkable-01",
        .polygon = {
            .outline = {
                {0.0, 0.0},
                {12.0, 0.0},
                {12.0, 8.0},
                {0.0, 8.0},
            },
        },
        .sourceIds = {"polyline-1"},
    });
    geometry.walls.push_back({
        .id = "wall-01",
        .segment = {
            .start = {0.0, 0.0},
            .end = {12.0, 0.0},
        },
        .sourceIds = {"line-1"},
    });
    geometry.obstacles.push_back({
        .id = "obstacle-01",
        .footprint = {
            .outline = {
                {4.0, 3.0},
                {5.0, 3.0},
                {5.0, 4.0},
                {4.0, 4.0},
            },
        },
        .sourceIds = {"polyline-obs"},
    });
    geometry.openings.push_back({
        .id = "opening-01",
        .kind = safecrowd::domain::OpeningKind::Exit,
        .span = {
            .start = {12.0, 3.0},
            .end = {12.0, 4.2},
        },
        .width = 1.2,
        .sourceIds = {"insert-1"},
    });

    safecrowd::domain::FacilityLayoutBuilder builder;
    const auto buildResult = builder.build(geometry);

    SC_EXPECT_EQ(buildResult.layout.id, std::string("layout-L1"));
    SC_EXPECT_EQ(buildResult.layout.zones.size(), std::size_t{2});
    SC_EXPECT_EQ(buildResult.layout.zones.front().kind, safecrowd::domain::ZoneKind::Room);
    SC_EXPECT_EQ(buildResult.layout.zones.back().kind, safecrowd::domain::ZoneKind::Exit);
    SC_EXPECT_EQ(buildResult.layout.connections.size(), std::size_t{1});
    SC_EXPECT_EQ(buildResult.layout.connections.front().kind, safecrowd::domain::ConnectionKind::Exit);
    SC_EXPECT_EQ(buildResult.layout.barriers.size(), std::size_t{2});
    SC_EXPECT_TRUE(buildResult.layout.barriers.back().geometry.closed);
    SC_EXPECT_TRUE(buildResult.issues.empty());
}

SC_TEST(ImportValidationServiceReportsMissingExitDisconnectedAreaAndNarrowConnections) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "layout-L1";
    layout.levelId = "L1";
    layout.zones.push_back({
        .id = "zone-room-1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room 1",
        .area = {
            .outline = {
                {0.0, 0.0},
                {4.0, 0.0},
                {4.0, 4.0},
                {0.0, 4.0},
            },
        },
    });
    layout.zones.push_back({
        .id = "zone-room-2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room 2",
        .area = {
            .outline = {
                {6.0, 0.0},
                {10.0, 0.0},
                {10.0, 4.0},
                {6.0, 4.0},
            },
        },
    });
    layout.connections.push_back({
        .id = "connection-1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "zone-room-1",
        .toZoneId = "zone-room-2",
        .effectiveWidth = 0.6,
    });

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(containsIssueCode(issues, safecrowd::domain::ImportIssueCode::MissingExit));
    SC_EXPECT_TRUE(containsIssueCode(issues, safecrowd::domain::ImportIssueCode::DisconnectedWalkableArea));
    SC_EXPECT_TRUE(containsIssueCode(issues, safecrowd::domain::ImportIssueCode::WidthBelowMinimum));
    SC_EXPECT_TRUE(safecrowd::domain::hasBlockingImportIssue(issues));
}

SC_TEST(FacilityLayoutBuilderInfersAdjacencyConnectionsBetweenTouchingWalkableZones) {
    safecrowd::domain::CanonicalGeometry geometry;
    geometry.levelId = "L2";
    geometry.walkableAreas.push_back({
        .id = "walkable-01",
        .polygon = {
            .outline = {
                {0.0, 0.0},
                {6.0, 0.0},
                {6.0, 4.0},
                {0.0, 4.0},
            },
        },
        .sourceIds = {"polyline-1"},
    });
    geometry.walkableAreas.push_back({
        .id = "walkable-02",
        .polygon = {
            .outline = {
                {6.0, 0.0},
                {12.0, 0.0},
                {12.0, 4.0},
                {6.0, 4.0},
            },
        },
        .sourceIds = {"polyline-2"},
    });
    geometry.openings.push_back({
        .id = "opening-exit",
        .kind = safecrowd::domain::OpeningKind::Exit,
        .span = {
            .start = {12.0, 1.2},
            .end = {12.0, 2.8},
        },
        .width = 1.6,
        .sourceIds = {"line-exit"},
    });

    safecrowd::domain::FacilityLayoutBuilder builder;
    const auto buildResult = builder.build(geometry);

    SC_EXPECT_EQ(buildResult.layout.zones.size(), std::size_t{3});
    SC_EXPECT_EQ(buildResult.layout.connections.size(), std::size_t{2});
    SC_EXPECT_EQ(buildResult.layout.connections.front().kind, safecrowd::domain::ConnectionKind::Exit);
    SC_EXPECT_EQ(buildResult.layout.connections.back().kind, safecrowd::domain::ConnectionKind::Opening);
    SC_EXPECT_NEAR(buildResult.layout.connections.back().effectiveWidth, 4.0, 1e-9);

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(buildResult.layout);

    SC_EXPECT_TRUE(!containsIssueCode(issues, safecrowd::domain::ImportIssueCode::DisconnectedWalkableArea));
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(issues));
}

SC_TEST(FacilityLayoutBuilderDoesNotInferAdjacencyAcrossWallSeam) {
    safecrowd::domain::CanonicalGeometry geometry;
    geometry.levelId = "L3";
    geometry.walkableAreas.push_back({
        .id = "walkable-01",
        .polygon = {
            .outline = {
                {0.0, 0.0},
                {6.0, 0.0},
                {6.0, 4.0},
                {0.0, 4.0},
            },
        },
        .sourceIds = {"polyline-1"},
    });
    geometry.walkableAreas.push_back({
        .id = "walkable-02",
        .polygon = {
            .outline = {
                {6.0, 0.0},
                {12.0, 0.0},
                {12.0, 4.0},
                {6.0, 4.0},
            },
        },
        .sourceIds = {"polyline-2"},
    });
    geometry.walls.push_back({
        .id = "wall-seam",
        .segment = {
            .start = {6.0, 0.0},
            .end = {6.0, 4.0},
        },
        .sourceIds = {"line-wall"},
    });
    geometry.openings.push_back({
        .id = "opening-exit",
        .kind = safecrowd::domain::OpeningKind::Exit,
        .span = {
            .start = {12.0, 1.2},
            .end = {12.0, 2.8},
        },
        .width = 1.6,
        .sourceIds = {"line-exit"},
    });

    safecrowd::domain::FacilityLayoutBuilder builder;
    const auto buildResult = builder.build(geometry);

    SC_EXPECT_EQ(buildResult.layout.connections.size(), std::size_t{1});
    SC_EXPECT_EQ(buildResult.layout.connections.front().kind, safecrowd::domain::ConnectionKind::Exit);

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(buildResult.layout);

    SC_EXPECT_TRUE(containsIssueCode(issues, safecrowd::domain::ImportIssueCode::DisconnectedWalkableArea));
    SC_EXPECT_TRUE(safecrowd::domain::hasBlockingImportIssue(issues));
}

SC_TEST(FacilityLayoutBuilderShrinksAdjacencyPortalToRemainingWallGap) {
    safecrowd::domain::CanonicalGeometry geometry;
    geometry.levelId = "L4";
    geometry.walkableAreas.push_back({
        .id = "walkable-01",
        .polygon = {
            .outline = {
                {0.0, 0.0},
                {6.0, 0.0},
                {6.0, 4.0},
                {0.0, 4.0},
            },
        },
        .sourceIds = {"polyline-1"},
    });
    geometry.walkableAreas.push_back({
        .id = "walkable-02",
        .polygon = {
            .outline = {
                {6.0, 0.0},
                {12.0, 0.0},
                {12.0, 4.0},
                {6.0, 4.0},
            },
        },
        .sourceIds = {"polyline-2"},
    });
    geometry.walls.push_back({
        .id = "wall-lower",
        .segment = {
            .start = {6.0, 0.0},
            .end = {6.0, 1.5},
        },
        .sourceIds = {"line-wall-1"},
    });
    geometry.walls.push_back({
        .id = "wall-upper",
        .segment = {
            .start = {6.0, 2.5},
            .end = {6.0, 4.0},
        },
        .sourceIds = {"line-wall-2"},
    });
    geometry.openings.push_back({
        .id = "opening-exit",
        .kind = safecrowd::domain::OpeningKind::Exit,
        .span = {
            .start = {12.0, 1.2},
            .end = {12.0, 2.8},
        },
        .width = 1.6,
        .sourceIds = {"line-exit"},
    });

    safecrowd::domain::FacilityLayoutBuilder builder;
    const auto buildResult = builder.build(geometry);

    SC_EXPECT_EQ(buildResult.layout.connections.size(), std::size_t{2});
    SC_EXPECT_EQ(buildResult.layout.connections.back().kind, safecrowd::domain::ConnectionKind::Opening);
    SC_EXPECT_NEAR(buildResult.layout.connections.back().effectiveWidth, 1.0, 1e-9);

    safecrowd::domain::ImportValidationService validator;
    const auto issues = validator.validate(buildResult.layout);

    SC_EXPECT_TRUE(!containsIssueCode(issues, safecrowd::domain::ImportIssueCode::DisconnectedWalkableArea));
}
