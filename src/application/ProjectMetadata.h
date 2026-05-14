#pragma once

#include <QString>

namespace safecrowd::application {

inline QString builtInDemoLayoutPath() {
    return QStringLiteral("safecrowd://demo/sprint1-facility");
}

inline QString builtInEvacuationScenarioDemoLayoutPath() {
    return QStringLiteral("safecrowd://demo/evacuation-scenario");
}

inline QString builtInTwoFloorEvacuationDemoLayoutPath() {
    return QStringLiteral("safecrowd://demo/two-floor-evacuation");
}

struct ProjectMetadata {
    QString name{};
    QString folderPath{};
    QString layoutPath{};
    QString savedAt{};

    bool isBuiltInDemo() const noexcept {
        return layoutPath == builtInDemoLayoutPath()
            || isBuiltInEvacuationScenarioDemo()
            || isBuiltInTwoFloorEvacuationDemo();
    }

    bool isBuiltInEvacuationScenarioDemo() const noexcept {
        return layoutPath == builtInEvacuationScenarioDemoLayoutPath();
    }

    bool isBuiltInTwoFloorEvacuationDemo() const noexcept {
        return layoutPath == builtInTwoFloorEvacuationDemoLayoutPath();
    }

    bool isBlankLayoutProject() const noexcept {
        return layoutPath.isEmpty();
    }

    bool isValid() const noexcept {
        if (isBuiltInDemo()) {
            return !name.isEmpty();
        }
        return !name.isEmpty() && !folderPath.isEmpty();
    }
};

inline ProjectMetadata makeBuiltInDemoProject() {
    return {
        .name = QStringLiteral("Demo"),
        .layoutPath = builtInDemoLayoutPath(),
    };
}

inline ProjectMetadata makeBuiltInEvacuationScenarioDemoProject() {
    return {
        .name = QStringLiteral("Evacuation Scenario Demo"),
        .layoutPath = builtInEvacuationScenarioDemoLayoutPath(),
    };
}

inline ProjectMetadata makeBuiltInTwoFloorEvacuationDemoProject() {
    return {
        .name = QStringLiteral("Two-floor Evacuation Demo"),
        .layoutPath = builtInTwoFloorEvacuationDemoLayoutPath(),
    };
}

}  // namespace safecrowd::application
