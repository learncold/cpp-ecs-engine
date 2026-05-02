#pragma once

#include <string>

#include "application/LayoutCanvasRendering.h"
#include "domain/FacilityLayout2D.h"

namespace safecrowd::application {

struct LayoutSnapOptions {
    double tolerancePixels{12.0};
    bool snapVertices{true};
    bool snapEdges{true};
};

struct LayoutSnapResult {
    safecrowd::domain::Point2D point{};
    bool snapped{false};
};

LayoutSnapResult snapLayoutPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& floorId,
    const safecrowd::domain::Point2D& point,
    const LayoutCanvasTransform& transform,
    const LayoutSnapOptions& options = {});

LayoutSnapResult snapLayoutDragPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& floorId,
    const safecrowd::domain::Point2D& anchor,
    const safecrowd::domain::Point2D& point,
    const LayoutCanvasTransform& transform,
    const LayoutSnapOptions& options = {});

}  // namespace safecrowd::application
