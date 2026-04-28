#include "domain/ScenarioRiskMetrics.h"

namespace safecrowd::domain {

const char* scenarioRiskLevelLabel(ScenarioRiskLevel level) noexcept {
    switch (level) {
    case ScenarioRiskLevel::Low:
        return "Low";
    case ScenarioRiskLevel::Medium:
        return "Medium";
    case ScenarioRiskLevel::High:
        return "High";
    }
    return "Low";
}

}  // namespace safecrowd::domain
