#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

enum class ImportedFileFormat {
    Unknown,
    Dxf,
    Ifc,
};

enum class ImportUnit {
    Unknown,
    Millimeter,
    Centimeter,
    Meter,
};

enum class RawEntityKind {
    Unknown,
    Line,
    Polyline,
    Polygon,
    BlockReference,
    IfcElement,
    Annotation,
};

struct SourceTrace {
    std::string sourceId{};
    std::string parentSourceId{};
    std::string layerName{};
    std::string objectName{};
    std::string externalId{};
};

struct RawTracedPolyline2D {
    SourceTrace trace{};
    Polyline2D geometry{};
    std::map<std::string, std::string> metadata{};
};

struct RawTracedPolygon2D {
    SourceTrace trace{};
    Polygon2D geometry{};
    std::map<std::string, std::string> metadata{};
};

struct RawBlockReference2D {
    std::string blockName{};
    Point2D insertionPoint{};
    double rotationRadians{0.0};
    double scaleX{1.0};
    double scaleY{1.0};
    std::vector<RawTracedPolyline2D> polylines{};
    std::vector<RawTracedPolygon2D> polygons{};
};

struct RawIfcElement2D {
    std::string elementType{};
    std::string representationId{};
    std::vector<Polyline2D> curves{};
    std::vector<Polygon2D> footprints{};
};

struct RawAnnotation2D {
    Point2D anchor{};
    std::string text{};
};

using RawEntityPayload = std::variant<
    std::monostate,
    LineSegment2D,
    Polyline2D,
    Polygon2D,
    RawBlockReference2D,
    RawIfcElement2D,
    RawAnnotation2D>;

struct RawEntity2D {
    RawEntityKind kind{RawEntityKind::Unknown};
    SourceTrace trace{};
    RawEntityPayload payload{};
    std::map<std::string, std::string> metadata{};
};

struct RawImportModel {
    ImportedFileFormat format{ImportedFileFormat::Unknown};
    ImportUnit unit{ImportUnit::Unknown};
    std::string sourceDocumentId{};
    std::string levelId{};
    std::vector<RawEntity2D> entities{};
    std::map<std::string, std::string> metadata{};
};

}  // namespace safecrowd::domain
