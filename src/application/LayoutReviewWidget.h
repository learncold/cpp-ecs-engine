#pragma once

#include <functional>

#include <QString>
#include <QWidget>

#include "domain/ImportResult.h"

namespace safecrowd::application {

class LayoutReviewWidget : public QWidget {
public:
    explicit LayoutReviewWidget(
        const QString& projectName,
        const safecrowd::domain::ImportResult& importResult,
        std::function<void()> saveProjectHandler,
        QWidget* parent = nullptr);
};

}  // namespace safecrowd::application
