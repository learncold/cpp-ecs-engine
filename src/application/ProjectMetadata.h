#pragma once

#include <QString>

namespace safecrowd::application {

struct ProjectMetadata {
    QString name{};
    QString folderPath{};
    QString layoutPath{};
    QString savedAt{};

    bool isValid() const noexcept {
        return !name.isEmpty() && !folderPath.isEmpty() && !layoutPath.isEmpty();
    }
};

}  // namespace safecrowd::application
