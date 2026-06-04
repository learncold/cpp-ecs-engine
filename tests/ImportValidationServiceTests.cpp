#include <algorithm>
#include <vector>

#include "TestSupport.h"

#include "domain/FacilityLayout2D.h"
#include "domain/ImportIssue.h"
#include "domain/ImportValidationService.h"

namespace {

using namespace safecrowd::domain;

// Builds a minimal but fully valid single-floor layout: one room reaching one
// exit through an aligned exit connection. validate() returns no issues for it,
// so any issue observed in a test is attributable to what that test adds.
FacilityLayout2D makeConnectedRoomAndExit() {
    FacilityLayout2D layout;
    layout.id = "layout";
    layout.floors.push_back({.id = "F1"});
    layout.zones.push_back({
        .id = "room",
        .floorId = "F1",
        .kind = ZoneKind::Room,
        .area = {.outline = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 8.0}, {0.0, 8.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .floorId = "F1",
        .kind = ZoneKind::Exit,
        .area = {.outline = {{10.0, 3.0}, {12.0, 3.0}, {12.0, 5.0}, {10.0, 5.0}}},
    });
    layout.connections.push_back({
        .id = "c1",
        .floorId = "F1",
        .kind = ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .centerSpan = {.start = {10.0, 4.0}, .end = {11.0, 4.0}},
    });
    return layout;
}

bool hasIssueCode(const std::vector<ImportIssue>& issues, ImportIssueCode code) {
    return std::any_of(issues.begin(), issues.end(), [&](const auto& issue) {
        return issue.code == code;
    });
}

}  // namespace

SC_TEST(ImportValidationProducesNoIssuesForCleanLayout) {
    const auto layout = makeConnectedRoomAndExit();

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(issues.empty());
}

SC_TEST(ImportValidationFlagsBarrierCrossingConnectionPassage) {
    auto layout = makeConnectedRoomAndExit();
    // Wall through the middle of the exit passage span (crosses at 50%).
    layout.barriers.push_back({
        .id = "wall",
        .floorId = "F1",
        .geometry = {.vertices = {{10.5, 3.0}, {10.5, 5.0}}, .closed = false},
        .blocksMovement = true,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
    // The obstruction is reported for review but must not block simulation.
    SC_EXPECT_TRUE(!hasBlockingImportIssue(issues));
}

SC_TEST(ImportValidationFlagsBarrierCrossingNearConnectionEndpoint) {
    auto layout = makeConnectedRoomAndExit();
    // Still inside the passage span, but close to the start endpoint.
    layout.barriers.push_back({
        .id = "near-start-wall",
        .floorId = "F1",
        .geometry = {.vertices = {{10.05, 3.0}, {10.05, 5.0}}, .closed = false},
        .blocksMovement = true,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
}

SC_TEST(ImportValidationFlagsCollinearBarrierOverConnectionPassage) {
    auto layout = makeConnectedRoomAndExit();
    // Unbroken wall segment left on the same span as the exit passage.
    layout.barriers.push_back({
        .id = "collinear-wall",
        .floorId = "F1",
        .geometry = {.vertices = {{10.2, 4.0}, {10.8, 4.0}}, .closed = false},
        .blocksMovement = true,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
}

SC_TEST(ImportValidationFlagsClosedObstacleContainingConnectionPassage) {
    auto layout = makeConnectedRoomAndExit();
    // Closed footprint fully contains the passage span, so no obstacle edge
    // crosses the span even though movement through the passage is blocked.
    layout.barriers.push_back({
        .id = "obstacle",
        .floorId = "F1",
        .geometry = {.vertices = {{9.5, 3.5}, {11.5, 3.5}, {11.5, 4.5}, {9.5, 4.5}}, .closed = true},
        .blocksMovement = true,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
}

SC_TEST(ImportValidationIgnoresNonBlockingBarrierOverConnection) {
    auto layout = makeConnectedRoomAndExit();
    layout.barriers.push_back({
        .id = "marking",
        .floorId = "F1",
        .geometry = {.vertices = {{10.5, 3.0}, {10.5, 5.0}}, .closed = false},
        .blocksMovement = false,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(!hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
}

SC_TEST(ImportValidationDoesNotFlagCollinearWallTouchingConnectionEndpoint) {
    auto layout = makeConnectedRoomAndExit();
    layout.barriers.push_back({
        .id = "collinear-flank",
        .floorId = "F1",
        .geometry = {.vertices = {{9.0, 4.0}, {10.0, 4.0}}, .closed = false},
        .blocksMovement = true,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(!hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
}

SC_TEST(ImportValidationDoesNotFlagWallMeetingConnectionAtEndpoint) {
    auto layout = makeConnectedRoomAndExit();
    // Boundary wall flanking the doorway: touches the span start endpoint only.
    layout.barriers.push_back({
        .id = "flank",
        .floorId = "F1",
        .geometry = {.vertices = {{10.0, 3.0}, {10.0, 5.0}}, .closed = false},
        .blocksMovement = true,
    });

    ImportValidationService validator;
    const auto issues = validator.validate(layout);

    SC_EXPECT_TRUE(!hasIssueCode(issues, ImportIssueCode::ObstructedConnection));
}
