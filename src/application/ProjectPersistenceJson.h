#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "domain/FacilityLayout2D.h"

namespace safecrowd::application {

QJsonArray pointArray(const safecrowd::domain::Point2D& point);
safecrowd::domain::Point2D pointFromJson(const QJsonValue& value);
QJsonArray ringToJson(const std::vector<safecrowd::domain::Point2D>& ring);
std::vector<safecrowd::domain::Point2D> ringFromJson(const QJsonArray& array);
QJsonObject polygonToJson(const safecrowd::domain::Polygon2D& polygon);
safecrowd::domain::Polygon2D polygonFromJson(const QJsonObject& object);
QJsonObject lineToJson(const safecrowd::domain::LineSegment2D& line);
safecrowd::domain::LineSegment2D lineFromJson(const QJsonObject& object);
QJsonObject polylineToJson(const safecrowd::domain::Polyline2D& polyline);
safecrowd::domain::Polyline2D polylineFromJson(const QJsonObject& object);
QJsonArray stringArray(const std::vector<std::string>& values);
std::vector<std::string> stringVectorFromJson(const QJsonArray& array);
QJsonValue optionalDoubleToJson(const std::optional<double>& value);
std::optional<double> optionalDoubleFromJson(const QJsonValue& value);

}  // namespace safecrowd::application
