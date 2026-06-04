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

inline QString builtInDemo2FLayoutPath() {
    return QStringLiteral("safecrowd://demo/demo-2f");
}

struct ProjectMetadata {
    QString name{};
    QString folderPath{};
    QString layoutPath{};
    QString savedAt{};

    bool isBuiltInDemo() const noexcept {
        return layoutPath == builtInDemoLayoutPath()
            || isBuiltInEvacuationScenarioDemo()
            || isBuiltInTwoFloorEvacuationDemo()
            || isBuiltInDemo2F();
    }

    bool isBuiltInEvacuationScenarioDemo() const noexcept {
        return layoutPath == builtInEvacuationScenarioDemoLayoutPath();
    }

    bool isBuiltInTwoFloorEvacuationDemo() const noexcept {
        return layoutPath == builtInTwoFloorEvacuationDemoLayoutPath();
    }

    bool isBuiltInDemo2F() const noexcept {
        return layoutPath == builtInDemo2FLayoutPath();
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

inline ProjectMetadata makeBuiltInDemo2FProject() {
    return {
        .name = QStringLiteral("Demo - 2F"),
        .layoutPath = builtInDemo2FLayoutPath(),
    };
}

}  // namespace safecrowd::application
