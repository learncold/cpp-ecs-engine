#include "application/ProjectPersistenceJson.h"

namespace safecrowd::application {
QJsonArray pointArray(const safecrowd::domain::Point2D& point) {
    return QJsonArray{point.x, point.y};
}

safecrowd::domain::Point2D pointFromJson(const QJsonValue& value) {
    const auto array = value.toArray();
    return {
        .x = array.size() > 0 ? array.at(0).toDouble() : 0.0,
        .y = array.size() > 1 ? array.at(1).toDouble() : 0.0,
    };
}

QJsonArray ringToJson(const std::vector<safecrowd::domain::Point2D>& ring) {
    QJsonArray array;
    for (const auto& point : ring) {
        array.append(pointArray(point));
    }
    return array;
}

std::vector<safecrowd::domain::Point2D> ringFromJson(const QJsonArray& array) {
    std::vector<safecrowd::domain::Point2D> ring;
    ring.reserve(array.size());
    for (const auto& value : array) {
        ring.push_back(pointFromJson(value));
    }
    return ring;
}

QJsonObject polygonToJson(const safecrowd::domain::Polygon2D& polygon) {
    QJsonObject object;
    object["outline"] = ringToJson(polygon.outline);

    QJsonArray holes;
    for (const auto& hole : polygon.holes) {
        holes.append(ringToJson(hole));
    }
    object["holes"] = holes;
    return object;
}

safecrowd::domain::Polygon2D polygonFromJson(const QJsonObject& object) {
    safecrowd::domain::Polygon2D polygon;
    polygon.outline = ringFromJson(object.value("outline").toArray());
    for (const auto& holeValue : object.value("holes").toArray()) {
        polygon.holes.push_back(ringFromJson(holeValue.toArray()));
    }
    return polygon;
}

QJsonObject lineToJson(const safecrowd::domain::LineSegment2D& line) {
    QJsonObject object;
    object["start"] = pointArray(line.start);
    object["end"] = pointArray(line.end);
    return object;
}

safecrowd::domain::LineSegment2D lineFromJson(const QJsonObject& object) {
    return {
        .start = pointFromJson(object.value("start")),
        .end = pointFromJson(object.value("end")),
    };
}

QJsonObject polylineToJson(const safecrowd::domain::Polyline2D& polyline) {
    QJsonObject object;
    object["vertices"] = ringToJson(polyline.vertices);
    object["closed"] = polyline.closed;
    return object;
}

safecrowd::domain::Polyline2D polylineFromJson(const QJsonObject& object) {
    return {
        .vertices = ringFromJson(object.value("vertices").toArray()),
        .closed = object.value("closed").toBool(false),
    };
}

QJsonArray stringArray(const std::vector<std::string>& values) {
    QJsonArray array;
    for (const auto& value : values) {
        array.append(QString::fromStdString(value));
    }
    return array;
}

std::vector<std::string> stringVectorFromJson(const QJsonArray& array) {
    std::vector<std::string> values;
    values.reserve(array.size());
    for (const auto& value : array) {
        values.push_back(value.toString().toStdString());
    }
    return values;
}

QJsonValue optionalDoubleToJson(const std::optional<double>& value) {
    if (!value.has_value()) {
        return QJsonValue();
    }
    return *value;
}

std::optional<double> optionalDoubleFromJson(const QJsonValue& value) {
    if (value.isDouble()) {
        return value.toDouble();
    }
    return std::nullopt;
}

}  // namespace safecrowd::application
