#include <filesystem>
#include <variant>
#include <vector>

#include "TestSupport.h"

#include "domain/ImportContracts.h"

namespace {

class FakeImportOrchestrator : public safecrowd::domain::ImportOrchestrator {
public:
    safecrowd::domain::ImportResult importFile(const safecrowd::domain::ImportRequest& request) override {
        lastRequest = request;

        safecrowd::domain::ImportResult result;
        result.layout = safecrowd::domain::FacilityLayout2D{
            .id = "layout-demo",
            .name = "Imported Demo Floor",
            .levelId = "L1",
        };
        return result;
    }

    safecrowd::domain::ImportRequest lastRequest{};
};

}  // namespace

SC_TEST(ImportContractsCaptureSprintOneLayoutFields) {
    safecrowd::domain::RawImportModel rawModel;
    rawModel.format = safecrowd::domain::ImportedFileFormat::Dxf;
    rawModel.unit = safecrowd::domain::ImportUnit::Meter;
    rawModel.sourceDocumentId = "demo-floor.dxf";
    rawModel.levelId = "L1";

    safecrowd::domain::RawEntity2D wallEntity;
    wallEntity.kind = safecrowd::domain::RawEntityKind::Polyline;
    wallEntity.trace.sourceId = "wall-01";
    wallEntity.trace.layerName = "WALL";
    wallEntity.payload = safecrowd::domain::Polyline2D{
        .vertices = {
            {0.0, 0.0},
            {10.0, 0.0},
        },
    };
    rawModel.entities.push_back(wallEntity);

    safecrowd::domain::RawEntity2D blockEntity;
    blockEntity.kind = safecrowd::domain::RawEntityKind::BlockReference;
    blockEntity.trace.sourceId = "block-01";
    blockEntity.payload = safecrowd::domain::RawBlockReference2D{
        .blockName = "STAIR_CORE",
        .insertionPoint = {2.0, 1.5},
        .rotationRadians = 0.25,
        .scaleX = 1.0,
        .scaleY = 1.0,
        .polylines = {
            {
                .vertices = {
                    {1.5, 1.0},
                    {2.5, 1.0},
                    {2.5, 2.0},
                    {1.5, 2.0},
                },
                .closed = true,
            },
        },
    };
    rawModel.entities.push_back(blockEntity);

    safecrowd::domain::CanonicalGeometry canonicalGeometry;
    canonicalGeometry.levelId = "L1";
    canonicalGeometry.walkableAreas.push_back({
        .id = "walkable-01",
        .polygon = {
            .outline = {
                {0.0, 0.0},
                {10.0, 0.0},
                {10.0, 6.0},
                {0.0, 6.0},
            },
        },
        .sourceIds = {"wall-01"},
    });
    canonicalGeometry.openings.push_back({
        .id = "opening-01",
        .kind = safecrowd::domain::OpeningKind::Exit,
        .span = {
            .start = {10.0, 2.0},
            .end = {10.0, 3.2},
        },
        .width = 1.2,
        .sourceIds = {"wall-01"},
    });

    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "layout-demo";
    layout.name = "Imported Demo Floor";
    layout.levelId = "L1";
    layout.zones.push_back({
        .id = "zone-room-a",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room A",
        .area = {
            .outline = {
                {0.0, 0.0},
                {5.0, 0.0},
                {5.0, 4.0},
                {0.0, 4.0},
            },
        },
        .defaultCapacity = 20,
        .provenance = {
            .sourceIds = {"wall-01"},
            .canonicalIds = {"walkable-01"},
        },
    });
    layout.zones.push_back({
        .id = "zone-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {
            .outline = {
                {9.0, 2.0},
                {10.0, 2.0},
                {10.0, 3.2},
                {9.0, 3.2},
            },
        },
        .defaultCapacity = 50,
        .provenance = {
            .sourceIds = {"wall-01"},
            .canonicalIds = {"opening-01"},
        },
    });
    layout.connections.push_back({
        .id = "conn-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "zone-room-a",
        .toZoneId = "zone-exit",
        .effectiveWidth = 1.2,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .centerSpan = {
            .start = {9.0, 2.6},
            .end = {10.0, 2.6},
        },
        .provenance = {
            .sourceIds = {"wall-01"},
            .canonicalIds = {"opening-01"},
        },
    });

    safecrowd::domain::ImportResult result;
    result.rawModel = rawModel;
    result.canonicalGeometry = canonicalGeometry;
    result.layout = layout;
    result.traceRefs.push_back({
        .targetId = "conn-exit",
        .sourceIds = {"wall-01"},
        .canonicalIds = {"opening-01"},
    });
    result.reviewStatus = safecrowd::domain::ImportReviewStatus::Approved;

    SC_EXPECT_EQ(result.rawModel->format, safecrowd::domain::ImportedFileFormat::Dxf);
    SC_EXPECT_EQ(result.canonicalGeometry->openings.front().kind, safecrowd::domain::OpeningKind::Exit);
    SC_EXPECT_EQ(result.layout->zones.front().defaultCapacity, std::size_t{20});
    SC_EXPECT_EQ(result.layout->connections.front().directionality, safecrowd::domain::TravelDirection::Bidirectional);
    SC_EXPECT_NEAR(result.layout->connections.front().effectiveWidth, 1.2, 1e-9);
    SC_EXPECT_TRUE(std::holds_alternative<safecrowd::domain::RawBlockReference2D>(result.rawModel->entities.back().payload));
    SC_EXPECT_EQ(result.layout->connections.front().provenance.canonicalIds.front(), std::string("opening-01"));
    SC_EXPECT_EQ(result.traceRefs.front().targetId, std::string("conn-exit"));
    SC_EXPECT_TRUE(result.readyForSimulation());
}

SC_TEST(ImportIssuesBlockSimulationOnlyForBlockingProblems) {
    std::vector<safecrowd::domain::ImportIssue> warnings = {
        {
            .severity = safecrowd::domain::ImportIssueSeverity::Warning,
            .code = safecrowd::domain::ImportIssueCode::WidthBelowMinimum,
            .message = "Exit width is below the demo threshold.",
            .sourceId = "opening-01",
        },
    };

    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(warnings));
    SC_EXPECT_EQ(std::string(safecrowd::domain::toString(warnings.front().severity)), std::string("Warning"));

    warnings.push_back({
        .severity = safecrowd::domain::ImportIssueSeverity::Error,
        .code = safecrowd::domain::ImportIssueCode::MissingExit,
        .message = "No reachable exit was inferred.",
        .targetId = "layout-demo",
    });

    SC_EXPECT_TRUE(safecrowd::domain::hasBlockingImportIssue(warnings));
    SC_EXPECT_TRUE(warnings.back().blocksSimulation());
}

SC_TEST(ImportResultRequiresApprovedReviewBeforeSimulation) {
    safecrowd::domain::ImportResult result;
    result.layout = safecrowd::domain::FacilityLayout2D{
        .id = "layout-demo",
        .levelId = "L1",
    };
    result.reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;

    SC_EXPECT_TRUE(!result.readyForSimulation());

    result.reviewStatus = safecrowd::domain::ImportReviewStatus::Approved;
    SC_EXPECT_TRUE(result.readyForSimulation());
}

SC_TEST(ImportOrchestratorUsesAFileBasedDomainEntryPoint) {
    FakeImportOrchestrator orchestrator;

    safecrowd::domain::ImportRequest request;
    request.sourcePath = std::filesystem::path("sample/demo-floor.dxf");
    request.requestedFormat = safecrowd::domain::ImportedFileFormat::Dxf;
    request.preserveRawModel = false;

    const auto result = orchestrator.importFile(request);

    SC_EXPECT_EQ(orchestrator.lastRequest.sourcePath.generic_string(), std::string("sample/demo-floor.dxf"));
    SC_EXPECT_EQ(orchestrator.lastRequest.requestedFormat, safecrowd::domain::ImportedFileFormat::Dxf);
    SC_EXPECT_TRUE(!orchestrator.lastRequest.preserveRawModel);
    SC_EXPECT_TRUE(result.layout.has_value());
}
