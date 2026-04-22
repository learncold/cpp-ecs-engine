#pragma once

#include <QString>
#include <QWidget>

#include "domain/ImportResult.h"

class QMouseEvent;
class QWheelEvent;

namespace safecrowd::application {

class LayoutPreviewWidget : public QWidget {
public:
    explicit LayoutPreviewWidget(safecrowd::domain::ImportResult importResult, QWidget* parent = nullptr);

    void focusIssueTarget(const QString& targetId);
    void resetView();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    safecrowd::domain::ImportResult importResult_{};
    QString focusedTargetId_{};
    QPointF panOffset_{};
    QPointF lastMousePosition_{};
    double zoom_{1.0};
    bool panning_{false};
};

}  // namespace safecrowd::application
