#pragma once

#include <QString>

namespace safecrowd::application {

inline QString builtInDemoLayoutPrefix() {
    return QStringLiteral("safecrowd://demo/");
}

inline QString sprint1DemoLayoutPath() {
    return QStringLiteral("safecrowd://demo/sprint1-facility");
}

inline QString twoFloorDemoLayoutPath() {
    return QStringLiteral("safecrowd://demo/2f-demo");
}

struct ProjectMetadata {
    QString name{};
    QString folderPath{};
    QString layoutPath{};
    QString savedAt{};

    bool isBuiltInDemo() const noexcept {
        return layoutPath.startsWith(builtInDemoLayoutPrefix());
    }

    bool isValid() const noexcept {
        if (isBuiltInDemo()) {
            return !name.isEmpty();
        }
        return !name.isEmpty() && !folderPath.isEmpty() && !layoutPath.isEmpty();
    }
};

}  // namespace safecrowd::application
