#pragma once

#include <QString>

namespace safecrowd::application {

inline QString builtInDemoLayoutPath() {
    return QStringLiteral("safecrowd://demo/sprint1-facility");
}

struct ProjectMetadata {
    QString name{};
    QString folderPath{};
    QString layoutPath{};
    QString savedAt{};

    bool isBuiltInDemo() const noexcept {
        return layoutPath == builtInDemoLayoutPath();
    }

    bool isValid() const noexcept {
        if (isBuiltInDemo()) {
            return !name.isEmpty();
        }
        return !name.isEmpty() && !folderPath.isEmpty() && !layoutPath.isEmpty();
    }
};

inline ProjectMetadata makeBuiltInDemoProject() {
    return {
        .name = QStringLiteral("Demo"),
        .layoutPath = builtInDemoLayoutPath(),
    };
}

}  // namespace safecrowd::application
