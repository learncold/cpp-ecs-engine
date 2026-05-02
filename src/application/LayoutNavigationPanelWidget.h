#pragma once

#include <functional>

#include <QSet>
#include <QString>
#include <QWidget>

#include "application/NavigationTreeWidget.h"
#include "domain/FacilityLayout2D.h"

namespace safecrowd::application {

class LayoutNavigationPanelWidget : public QWidget {
public:
    explicit LayoutNavigationPanelWidget(
        const safecrowd::domain::FacilityLayout2D* layout,
        std::function<void(const QString&)> selectElementHandler = {},
        QWidget* parent = nullptr,
        QWidget* headerWidget = nullptr,
        NavigationTreeState navigationState = {},
        std::function<void(const QSet<QString>&)> expandedStateChangedHandler = {});
};

}  // namespace safecrowd::application
