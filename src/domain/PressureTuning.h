#pragma once

namespace safecrowd::domain {

inline constexpr double kPressureHotspotCellSizeMeters = 1.5;
inline constexpr double kPressureReferenceDistanceMeters = 1.0;
inline constexpr double kPressureHighDensityThresholdPeoplePerSquareMeter = 3.55;
inline constexpr float kPressureCriticalScoreThreshold = 1.0f;
inline constexpr float kPressureCriticalExposureThresholdSeconds = 2.0f;

}  // namespace safecrowd::domain
