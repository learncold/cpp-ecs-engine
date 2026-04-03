#pragma once

#include <string>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

enum class OpeningKind {
    Unknown,
    Doorway,
    Passage,
    Exit,
};

enum class VerticalLinkKind {
    Unknown,
    Stair,
    Ramp,
    Elevator,
};

struct WalkableSurface2D {
    std::string id{};
    Polygon2D polygon{};
    std::vector<std::string> sourceIds{};
};

struct WallSegment2D {
    std::string id{};
    LineSegment2D segment{};
    double thickness{0.0};
    std::vector<std::string> sourceIds{};
};

struct Opening2D {
    std::string id{};
    OpeningKind kind{OpeningKind::Unknown};
    LineSegment2D span{};
    double width{0.0};
    std::vector<std::string> sourceIds{};
};

struct Obstacle2D {
    std::string id{};
    Polygon2D footprint{};
    std::vector<std::string> sourceIds{};
};

struct VerticalLink2D {
    std::string id{};
    VerticalLinkKind kind{VerticalLinkKind::Unknown};
    Point2D anchor{};
    std::string targetLevelId{};
    double width{0.0};
    std::vector<std::string> sourceIds{};
};

struct CanonicalGeometry {
    std::string levelId{};
    std::vector<WalkableSurface2D> walkableAreas{};
    std::vector<WallSegment2D> walls{};
    std::vector<Opening2D> openings{};
    std::vector<Obstacle2D> obstacles{};
    std::vector<VerticalLink2D> verticalLinks{};
};

}  // namespace safecrowd::domain
