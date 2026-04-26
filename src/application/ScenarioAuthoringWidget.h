#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace safecrowd::application {

class LayoutPreviewWidget;
class WorkspaceShell;

class ScenarioAuthoringWidget : public QWidget {
public:
    explicit ScenarioAuthoringWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        QWidget* parent = nullptr);

private:
    enum class NavigationView {
        Layout,
        Crowd,
    };

    void addEventDraft(const QString& name, const QString& trigger, const QString& target);
    void refreshDraftSummary();
    void refreshNavigationPanel();
    void refreshReadiness();
    void showInitialForm();
    void showScenarioCanvas();
    safecrowd::domain::ScenarioDraft currentDraft() const;

    QString projectName_{};
    QString selectedStartText_{};
    QString selectedDestinationText_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::ScenarioDraft scenarioDraft_{};
    std::vector<safecrowd::domain::OperationalEventDraft> events_{};
    WorkspaceShell* shell_{nullptr};
    LayoutPreviewWidget* preview_{nullptr};
    QLineEdit* scenarioNameEdit_{nullptr};
    QComboBox* startZoneComboBox_{nullptr};
    QComboBox* destinationComboBox_{nullptr};
    QSpinBox* occupantCountSpinBox_{nullptr};
    QLabel* draftSummaryLabel_{nullptr};
    QLabel* readinessLabel_{nullptr};
    QPushButton* createScenarioButton_{nullptr};
    NavigationView navigationView_{NavigationView::Layout};
};

}  // namespace safecrowd::application
