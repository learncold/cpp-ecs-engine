#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

enum class ZoneKind {
    Unknown = 0,
    Room = 1,
    Exit = 3,
    Intersection = 4,
    Stair = 5,
};

enum class ConnectionKind {
    Unknown,
    Doorway,
    Opening,
    Exit,
    Stair,
    Ramp,
};

enum class TravelDirection {
    Bidirectional,
    ForwardOnly,
    ReverseOnly,
    Closed,
};

enum class StairEntryDirection {
    Unspecified,
    North,
    East,
    South,
    West,
};

enum class ControlKind {
    Unknown,
    Gate,
    ExitControl,
    BarrierToggle,
};

struct ElementProvenance {
    std::vector<std::string> sourceIds{};
    std::vector<std::string> canonicalIds{};
};

struct Floor2D {
    std::string id{};
    std::string label{};
    double elevationMeters{0.0};
    ElementProvenance provenance{};
};

struct Zone2D {
    std::string id{};
    std::string floorId{};
    ZoneKind kind{ZoneKind::Unknown};
    std::string label{};
    Polygon2D area{};
    std::size_t defaultCapacity{0};
    bool isStair{false};
    bool isRamp{false};
    ElementProvenance provenance{};
};

struct Connection2D {
    std::string id{};
    std::string floorId{};
    ConnectionKind kind{ConnectionKind::Unknown};
    std::string fromZoneId{};
    std::string toZoneId{};
    double effectiveWidth{0.0};
    TravelDirection directionality{TravelDirection::Bidirectional};
    bool isStair{false};
    bool isRamp{false};
    StairEntryDirection lowerEntryDirection{StairEntryDirection::Unspecified};
    StairEntryDirection upperEntryDirection{StairEntryDirection::Unspecified};
    LineSegment2D centerSpan{};
    ElementProvenance provenance{};
};

struct Barrier2D {
    std::string id{};
    std::string floorId{};
    Polyline2D geometry{};
    bool blocksMovement{true};
    ElementProvenance provenance{};
};

struct ControlPoint2D {
    std::string id{};
    std::string floorId{};
    ControlKind kind{ControlKind::Unknown};
    std::string targetId{};
    bool defaultOpen{true};
    ElementProvenance provenance{};
};

struct FacilityLayout2D {
    std::string id{};
    std::string name{};
    std::string levelId{};
    std::vector<Floor2D> floors{};
    std::vector<Zone2D> zones{};
    std::vector<Connection2D> connections{};
    std::vector<Barrier2D> barriers{};
    std::vector<ControlPoint2D> controls{};
};

}  // namespace safecrowd::domain
