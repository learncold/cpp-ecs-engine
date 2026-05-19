#include "TestSupport.h"

#include "application/LayoutCanvasSnapping.h"

#include <vector>

#include <QPointF>
#include <QRectF>

namespace {

safecrowd::domain::Polygon2D rectangle(double minX, double minY, double maxX, double maxY) {
    return {
        .outline = {
            {.x = minX, .y = minY},
            {.x = maxX, .y = minY},
            {.x = maxX, .y = maxY},
            {.x = minX, .y = maxY},
        },
    };
}

}  // namespace

SC_TEST(LayoutCanvasSnapping_SelectionDragAcceptsIndependentAxisSnaps) {
    safecrowd::domain::FacilityLayout2D layout{
        .floors = {{.id = "L1", .label = "L1"}},
        .zones = {{
            .id = "target",
            .floorId = "L1",
            .area = rectangle(10.0, 10.0, 12.0, 12.0),
        }},
    };
    const safecrowd::application::LayoutCanvasBounds bounds{
        .minX = 0.0,
        .minY = 0.0,
        .maxX = 20.0,
        .maxY = 20.0,
    };
    const safecrowd::application::LayoutCanvasTransform transform(bounds, QRectF(0.0, 0.0, 200.0, 200.0), 1.0, {});
    const std::vector<safecrowd::domain::Point2D> anchors{{.x = 0.0, .y = 0.0}};

    const auto snapped = safecrowd::application::snapLayoutSelectionDrag(
        layout,
        "L1",
        anchors,
        {.x = 8.9, .y = 8.9},
        transform,
        {});

    SC_EXPECT_TRUE(snapped.snapped);
    SC_EXPECT_NEAR(snapped.delta.x, 10.0, 1e-9);
    SC_EXPECT_NEAR(snapped.delta.y, 10.0, 1e-9);
}
