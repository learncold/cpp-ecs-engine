#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/LayoutPreviewWidget.h"
#include "domain/ImportResult.h"

class QLabel;
class QPushButton;

namespace safecrowd::application {

class WorkspaceShell;

class LayoutReviewWidget : public QWidget {
public:
    explicit LayoutReviewWidget(
        const QString& projectName,
        const safecrowd::domain::ImportResult& importResult,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void(const safecrowd::domain::ImportResult&)> approvalHandler,
        QWidget* parent = nullptr);

    const safecrowd::domain::ImportResult& currentImportResult() const noexcept;
    bool undoLastEdit();

private:
    enum class NavigationView {
        Issues,
        Layout,
    };

    void handleIssueSelected(const safecrowd::domain::ImportIssue& issue);
    void handleLayoutElementSelected(const QString& elementId);
    void handleLayoutEdited(const safecrowd::domain::FacilityLayout2D& layout);
    void handlePreviewSelectionChanged(const PreviewSelection& selection);
    void refreshApprovalState();
    void refreshNavigationPanel();
    void restoreInspectorState();
    void showDefaultInspector();
    void showIssueInspector(const safecrowd::domain::ImportIssue& issue);
    void showSelectionInspector(const PreviewSelection& selection);
    void updateValidatedIssues();
    void applyImportResultState();

    QString projectName_{};
    safecrowd::domain::ImportResult importResult_{};
    std::function<void()> openProjectHandler_{};
    std::function<void(const safecrowd::domain::ImportResult&)> approvalHandler_{};
    std::vector<safecrowd::domain::FacilityLayout2D> undoHistory_{};
    WorkspaceShell* shell_{nullptr};
    LayoutPreviewWidget* preview_{nullptr};
    QLabel* inspectorTitleLabel_{nullptr};
    QLabel* inspectorDetailLabel_{nullptr};
    QLabel* approvalStatusLabel_{nullptr};
    QPushButton* approveButton_{nullptr};
    NavigationView navigationView_{NavigationView::Issues};
    QString selectedIssueTargetId_{};
    QString selectedIssueCode_{};
    PreviewSelection lastSelection_{};
};

}  // namespace safecrowd::application
