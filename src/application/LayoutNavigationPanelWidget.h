#pragma once

#include <functional>

#include <QString>
#include <QWidget>

#include "domain/FacilityLayout2D.h"

namespace safecrowd::application {

class LayoutNavigationPanelWidget : public QWidget {
public:
    explicit LayoutNavigationPanelWidget(
        const safecrowd::domain::FacilityLayout2D* layout,
        std::function<void(const QString&)> selectElementHandler = {},
        QWidget* parent = nullptr,
        QWidget* headerWidget = nullptr);
};

}  // namespace safecrowd::application
