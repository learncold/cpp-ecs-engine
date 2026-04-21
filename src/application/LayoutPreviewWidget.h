#pragma once

#include <QWidget>

#include "domain/ImportResult.h"

namespace safecrowd::application {

class LayoutPreviewWidget : public QWidget {
public:
    explicit LayoutPreviewWidget(safecrowd::domain::ImportResult importResult, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    safecrowd::domain::ImportResult importResult_{};
};

}  // namespace safecrowd::application
