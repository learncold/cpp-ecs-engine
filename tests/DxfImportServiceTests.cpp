#include <filesystem>
#include <fstream>
#include <string>

#include "TestSupport.h"

#include "domain/DxfImportService.h"

namespace {

std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path, std::ios::trunc);
    output << content;
    output.close();
    return path;
}

const char* kHappyPathDxf = R"(0
SECTION
2
HEADER
9
$INSUNITS
70
6
0
ENDSEC
0
SECTION
2
BLOCKS
0
BLOCK
2
EXIT_PORTAL
8
0
0
LWPOLYLINE
8
0
90
2
70
0
10
0
20
0
10
1.2
20
0
0
ENDBLK
0
BLOCK
2
OBSTACLE_BOX
8
0
0
LWPOLYLINE
8
0
90
4
70
1
10
0
20
0
10
1
20
0
10
1
20
1
10
0
20
1
0
ENDBLK
0
ENDSEC
0
SECTION
2
ENTITIES
0
LWPOLYLINE
8
WALKABLE
90
4
70
1
10
0
20
0
10
12
20
0
10
12
20
8
10
0
20
8
0
LWPOLYLINE
8
WALL
90
4
70
1
10
0
20
0
10
12
20
0
10
12
20
8
10
0
20
8
0
INSERT
8
EXIT
2
EXIT_PORTAL
10
12
20
3
41
1
42
1
50
0
0
INSERT
8
OBSTACLE
2
OBSTACLE_BOX
10
4
20
3
41
1.5
42
1
50
0
0
ENDSEC
0
EOF
)";

const char* kMissingBlockDxf = R"(0
SECTION
2
ENTITIES
0
INSERT
8
EXIT
2
MISSING_BLOCK
10
1
20
2
0
ENDSEC
0
EOF
)";

const char* kClassicPolylineDxf = R"(0
SECTION
2
HEADER
9
$INSUNITS
70
6
0
ENDSEC
0
SECTION
2
ENTITIES
0
POLYLINE
8
WALKABLE
70
1
0
VERTEX
10
0
20
0
0
VERTEX
10
6
20
0
0
VERTEX
10
6
20
4
0
VERTEX
10
0
20
4
0
SEQEND
0
POLYLINE
8
WALL
70
0
0
VERTEX
10
0
20
0
0
VERTEX
10
6
20
0
0
SEQEND
0
EOF
)";

}  // namespace

SC_TEST(DxfImportServiceBuildsCanonicalGeometryFromHappyPathSample) {
    const auto sourcePath = writeTempFile("safecrowd-happy-path.dxf", kHappyPathDxf);

    safecrowd::domain::DxfImportService importer;
    safecrowd::domain::ImportRequest request;
    request.sourcePath = sourcePath;
    request.requestedFormat = safecrowd::domain::ImportedFileFormat::Dxf;

    const auto result = importer.importFile(request);

    SC_EXPECT_TRUE(result.rawModel.has_value());
    SC_EXPECT_TRUE(result.canonicalGeometry.has_value());
    SC_EXPECT_EQ(result.rawModel->unit, safecrowd::domain::ImportUnit::Meter);
    SC_EXPECT_EQ(result.rawModel->entities.size(), std::size_t{4});
    SC_EXPECT_EQ(result.canonicalGeometry->walkableAreas.size(), std::size_t{1});
    SC_EXPECT_EQ(result.canonicalGeometry->walls.size(), std::size_t{4});
    SC_EXPECT_EQ(result.canonicalGeometry->openings.size(), std::size_t{1});
    SC_EXPECT_EQ(result.canonicalGeometry->obstacles.size(), std::size_t{1});
    SC_EXPECT_EQ(result.canonicalGeometry->openings.front().kind, safecrowd::domain::OpeningKind::Exit);
    SC_EXPECT_NEAR(result.canonicalGeometry->openings.front().width, 1.2, 1e-9);
    SC_EXPECT_EQ(result.traceRefs.size(), std::size_t{7});
    SC_EXPECT_EQ(result.traceRefs.front().canonicalIds.front(), result.traceRefs.front().targetId);
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(result.issues));
    SC_EXPECT_EQ(result.reviewStatus, safecrowd::domain::ImportReviewStatus::Pending);

    std::filesystem::remove(sourcePath);
}

SC_TEST(DxfImportServiceReportsMissingBlockDefinitions) {
    const auto sourcePath = writeTempFile("safecrowd-missing-block.dxf", kMissingBlockDxf);

    safecrowd::domain::DxfImportService importer;
    safecrowd::domain::ImportRequest request;
    request.sourcePath = sourcePath;
    request.requestedFormat = safecrowd::domain::ImportedFileFormat::Dxf;

    const auto result = importer.importFile(request);

    SC_EXPECT_TRUE(result.rawModel.has_value());
    SC_EXPECT_TRUE(result.canonicalGeometry.has_value());
    SC_EXPECT_TRUE(safecrowd::domain::hasBlockingImportIssue(result.issues));
    SC_EXPECT_EQ(result.issues.front().code, safecrowd::domain::ImportIssueCode::MissingBlockDefinition);
    SC_EXPECT_EQ(result.reviewStatus, safecrowd::domain::ImportReviewStatus::Rejected);

    std::filesystem::remove(sourcePath);
}

SC_TEST(DxfImportServiceImportsClassicPolylineEntities) {
    const auto sourcePath = writeTempFile("safecrowd-classic-polyline.dxf", kClassicPolylineDxf);

    safecrowd::domain::DxfImportService importer;
    safecrowd::domain::ImportRequest request;
    request.sourcePath = sourcePath;
    request.requestedFormat = safecrowd::domain::ImportedFileFormat::Dxf;

    const auto result = importer.importFile(request);

    SC_EXPECT_TRUE(result.rawModel.has_value());
    SC_EXPECT_TRUE(result.canonicalGeometry.has_value());
    SC_EXPECT_EQ(result.rawModel->entities.size(), std::size_t{2});
    SC_EXPECT_EQ(result.canonicalGeometry->walkableAreas.size(), std::size_t{1});
    SC_EXPECT_EQ(result.canonicalGeometry->walls.size(), std::size_t{1});
    SC_EXPECT_EQ(result.traceRefs.size(), std::size_t{2});
    SC_EXPECT_TRUE(!safecrowd::domain::hasBlockingImportIssue(result.issues));

    std::filesystem::remove(sourcePath);
}
