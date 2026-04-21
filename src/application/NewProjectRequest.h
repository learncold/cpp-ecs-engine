#pragma once

#include <QString>

namespace safecrowd::application {

struct NewProjectRequest {
    QString projectName{};
    QString layoutPath{};
    QString folderPath{};
};

}  // namespace safecrowd::application
