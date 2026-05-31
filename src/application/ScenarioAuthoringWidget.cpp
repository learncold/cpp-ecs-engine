#include "application/ScenarioAuthoringWidget.h"

#include "domain/GeometryQueries.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "application/LayoutNavigationPanelWidget.h"
#include "application/NavigationTreeWidget.h"
#include "application/ScenarioCanvasWidget.h"
#include "application/ScenarioRunWidget.h"
#include "application/ToolIconResources.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

using safecrowd::domain::pointInPolygon;

constexpr double kDefaultRunTimeLimitSeconds = 60.0;
constexpr double kDefaultRunSampleIntervalSeconds = 1.0;
constexpr int kMaxUiSeed = 2147483647;

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

bool operationalEventHistoryShortcutBlockedByTextInput() {
    auto* focused = QApplication::focusWidget();
    return qobject_cast<QLineEdit*>(focused) != nullptr
        || qobject_cast<QPlainTextEdit*>(focused) != nullptr;
}

bool pointsEqual(
    const std::vector<safecrowd::domain::Point2D>& lhs,
    const std::vector<safecrowd::domain::Point2D>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].x != rhs[index].x || lhs[index].y != rhs[index].y) {
            return false;
        }
    }
    return true;
}

bool crowdPlacementEqual(const ScenarioCrowdPlacement& lhs, const ScenarioCrowdPlacement& rhs) {
    return lhs.id == rhs.id
        && lhs.name == rhs.name
        && lhs.kind == rhs.kind
        && lhs.zoneId == rhs.zoneId
        && lhs.floorId == rhs.floorId
        && pointsEqual(lhs.area, rhs.area)
        && lhs.occupantCount == rhs.occupantCount
        && lhs.velocity.x == rhs.velocity.x
        && lhs.velocity.y == rhs.velocity.y
        && lhs.distribution == rhs.distribution
        && pointsEqual(lhs.generatedPositions, rhs.generatedPositions)
        && lhs.sourceAgentsPerSpawn == rhs.sourceAgentsPerSpawn
        && lhs.sourceStartSeconds == rhs.sourceStartSeconds
        && lhs.sourceEndSeconds == rhs.sourceEndSeconds
        && lhs.sourceIntervalSeconds == rhs.sourceIntervalSeconds;
}

bool crowdPlacementsEqual(
    const std::vector<ScenarioCrowdPlacement>& lhs,
    const std::vector<ScenarioCrowdPlacement>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!crowdPlacementEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

bool connectionBlockIntervalsEqual(
    const std::vector<safecrowd::domain::ConnectionBlockIntervalDraft>& lhs,
    const std::vector<safecrowd::domain::ConnectionBlockIntervalDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].startSeconds != rhs[index].startSeconds
            || lhs[index].endSeconds != rhs[index].endSeconds) {
            return false;
        }
    }
    return true;
}

bool connectionBlockEqual(
    const safecrowd::domain::ConnectionBlockDraft& lhs,
    const safecrowd::domain::ConnectionBlockDraft& rhs) {
    return lhs.id == rhs.id
        && lhs.connectionId == rhs.connectionId
        && connectionBlockIntervalsEqual(lhs.intervals, rhs.intervals);
}

bool connectionBlocksEqual(
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& lhs,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!connectionBlockEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

bool environmentHazardEqual(
    const safecrowd::domain::EnvironmentHazardDraft& lhs,
    const safecrowd::domain::EnvironmentHazardDraft& rhs) {
    return lhs.id == rhs.id
        && lhs.kind == rhs.kind
        && lhs.name == rhs.name
        && lhs.affectedZoneId == rhs.affectedZoneId
        && lhs.floorId == rhs.floorId
        && lhs.position.x == rhs.position.x
        && lhs.position.y == rhs.position.y
        && lhs.startSeconds == rhs.startSeconds
        && lhs.endSeconds == rhs.endSeconds
        && lhs.severity == rhs.severity
        && safecrowd::domain::environmentHazardRadiusMeters(lhs)
            == safecrowd::domain::environmentHazardRadiusMeters(rhs)
        && lhs.note == rhs.note;
}

bool environmentHazardsEqual(
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& lhs,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!environmentHazardEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

bool routeGuidancePeriodsEqual(
    const std::vector<safecrowd::domain::RouteGuidancePeriodDraft>& lhs,
    const std::vector<safecrowd::domain::RouteGuidancePeriodDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].startSeconds != rhs[index].startSeconds
            || lhs[index].endSeconds != rhs[index].endSeconds) {
            return false;
        }
    }
    return true;
}

bool routeGuidanceEqual(
    const safecrowd::domain::RouteGuidanceDraft& lhs,
    const safecrowd::domain::RouteGuidanceDraft& rhs) {
    return lhs.id == rhs.id
        && lhs.startSeconds == rhs.startSeconds
        && lhs.endSeconds == rhs.endSeconds
        && routeGuidancePeriodsEqual(lhs.periods, rhs.periods)
        && lhs.guidedExitZoneId == rhs.guidedExitZoneId
        && lhs.installConnectionId == rhs.installConnectionId
        && lhs.installFloorId == rhs.installFloorId
        && lhs.installZoneId == rhs.installZoneId
        && lhs.installPosition.x == rhs.installPosition.x
        && lhs.installPosition.y == rhs.installPosition.y
        && lhs.baseComplianceRate == rhs.baseComplianceRate
        && lhs.influenceRadiusMeters == rhs.influenceRadiusMeters
        && lhs.maxDetourMeters == rhs.maxDetourMeters;
}

bool routeGuidancesEqual(
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& lhs,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!routeGuidanceEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

bool editOperationalEvent(
    safecrowd::domain::OperationalEventDraft* event,
    QWidget* parent) {
    if (event == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Edit event");

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(QString::fromStdString(event->name));
    auto* triggerEdit = new QPlainTextEdit(&dialog);
    triggerEdit->setPlainText(QString::fromStdString(event->triggerSummary));
    triggerEdit->setMinimumHeight(72);
    auto* targetEdit = new QPlainTextEdit(&dialog);
    targetEdit->setPlainText(QString::fromStdString(event->targetSummary));
    targetEdit->setMinimumHeight(72);

    form->addRow("Name", nameEdit);
    form->addRow("Trigger", triggerEdit);
    form->addRow("Target", targetEdit);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const auto name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        return false;
    }

    event->name = name.toStdString();
    event->triggerSummary = triggerEdit->toPlainText().trimmed().toStdString();
    event->targetSummary = targetEdit->toPlainText().trimmed().toStdString();
    return true;
}

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

QString zoneName(const safecrowd::domain::FacilityLayout2D& layout, const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    if (it == layout.zones.end()) {
        return QString::fromStdString(zoneId);
    }
    const auto label = QString::fromStdString(it->label);
    return label.isEmpty() ? QString::fromStdString(it->id) : label;
}

QString connectionLabel(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection) {
    const auto from = zoneName(layout, connection.fromZoneId);
    const auto to = zoneName(layout, connection.toZoneId);
    if (!from.isEmpty() && !to.isEmpty()) {
        return QString("%1 -> %2").arg(from, to);
    }
    return QString::fromStdString(connection.id);
}

QString connectionLabelForId(const safecrowd::domain::FacilityLayout2D& layout, const std::string& connectionId) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return connection.id == connectionId;
    });
    if (it == layout.connections.end()) {
        return QString::fromStdString(connectionId);
    }
    return connectionLabel(layout, *it);
}

QString blockScheduleSummary(const safecrowd::domain::ConnectionBlockDraft& block) {
    if (block.intervals.empty()) {
        return "Always blocked";
    }

    QStringList intervals;
    for (const auto& interval : block.intervals) {
        intervals << QString("%1s - %2s").arg(interval.startSeconds, 0, 'f', 1).arg(interval.endSeconds, 0, 'f', 1);
    }
    return intervals.join(", ");
}

QString hazardKindLabel(safecrowd::domain::EnvironmentHazardKind kind) {
    switch (kind) {
    case safecrowd::domain::EnvironmentHazardKind::Smoke:
        return "Smoke";
    case safecrowd::domain::EnvironmentHazardKind::Fire:
    default:
        return "Fire";
    }
}

QString severityLabel(safecrowd::domain::ScenarioElementSeverity severity) {
    switch (severity) {
    case safecrowd::domain::ScenarioElementSeverity::Low:
        return "Low";
    case safecrowd::domain::ScenarioElementSeverity::High:
        return "High";
    case safecrowd::domain::ScenarioElementSeverity::Medium:
    default:
        return "Medium";
    }
}

QString hazardScheduleSummary(const safecrowd::domain::EnvironmentHazardDraft& hazard) {
    const auto start = std::max(0.0, hazard.startSeconds);
    if (safecrowd::domain::environmentHazardHasOpenEndedSchedule(hazard)) {
        return QString("%1s - open").arg(start, 0, 'f', 1);
    }
    return QString("%1s - %2s").arg(start, 0, 'f', 1).arg(std::max(start, hazard.endSeconds), 0, 'f', 1);
}

QString hazardZoneSummary(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::EnvironmentHazardDraft& hazard) {
    if (hazard.affectedZoneId.empty()) {
        return "Unassigned";
    }
    return zoneName(layout, hazard.affectedZoneId);
}

QString hazardPositionSummary(const safecrowd::domain::EnvironmentHazardDraft& hazard) {
    return QString("(%1, %2)").arg(hazard.position.x, 0, 'f', 1).arg(hazard.position.y, 0, 'f', 1);
}

QString routeGuidancePositionSummary(const safecrowd::domain::RouteGuidanceDraft& guidance) {
    if (guidance.installFloorId.empty() && guidance.installZoneId.empty()) {
        return guidance.installConnectionId.empty()
            ? QStringLiteral("Derived from target")
            : QStringLiteral("Derived from install anchor");
    }
    return QString("(%1, %2)")
        .arg(guidance.installPosition.x, 0, 'f', 1)
        .arg(guidance.installPosition.y, 0, 'f', 1);
}

QString routeGuidanceInstallLabel(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::RouteGuidanceDraft& guidance) {
    if (!guidance.installConnectionId.empty()) {
        return connectionLabelForId(layout, guidance.installConnectionId);
    }
    if (!guidance.installZoneId.empty()) {
        return zoneName(layout, guidance.installZoneId);
    }
    return QStringLiteral("Unassigned");
}

bool hasSmokeHazard(const safecrowd::domain::EnvironmentState& environment) {
    return std::any_of(environment.hazards.begin(), environment.hazards.end(), [](const auto& hazard) {
        return hazard.kind == safecrowd::domain::EnvironmentHazardKind::Smoke;
    });
}

bool editEnvironmentHazard(
    safecrowd::domain::EnvironmentHazardDraft* hazard,
    const safecrowd::domain::FacilityLayout2D& layout,
    QWidget* parent) {
    if (hazard == nullptr) {
        return false;
    }
    if (layout.zones.empty()) {
        QMessageBox::warning(parent, "Edit hazard", "A hazard must be assigned to a zone.");
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Edit hazard");

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(QString::fromStdString(hazard->name));

    auto* zoneCombo = new QComboBox(&dialog);
    for (const auto& zone : layout.zones) {
        zoneCombo->addItem(zoneLabel(zone), QString::fromStdString(zone.id));
    }
    zoneCombo->setCurrentIndex(std::max(0, zoneCombo->findData(QString::fromStdString(hazard->affectedZoneId))));

    auto* xSpin = new QDoubleSpinBox(&dialog);
    xSpin->setRange(-100000.0, 100000.0);
    xSpin->setDecimals(2);
    xSpin->setValue(hazard->position.x);

    auto* ySpin = new QDoubleSpinBox(&dialog);
    ySpin->setRange(-100000.0, 100000.0);
    ySpin->setDecimals(2);
    ySpin->setValue(hazard->position.y);

    auto* startSpin = new QDoubleSpinBox(&dialog);
    startSpin->setRange(0.0, 86400.0);
    startSpin->setDecimals(1);
    startSpin->setSuffix(" s");
    const auto initialStartSeconds = std::max(0.0, hazard->startSeconds);
    const auto initialOpenEnded = safecrowd::domain::environmentHazardHasOpenEndedSchedule(*hazard);
    startSpin->setValue(initialStartSeconds);

    auto* endSpin = new QDoubleSpinBox(&dialog);
    endSpin->setRange(0.0, 86400.0);
    endSpin->setDecimals(1);
    endSpin->setSuffix(" s");
    endSpin->setValue(initialOpenEnded
        ? initialStartSeconds
        : std::max(initialStartSeconds, hazard->endSeconds));

    auto* openEndedCheck = new QCheckBox("Open ended", &dialog);
    openEndedCheck->setChecked(initialOpenEnded);
    endSpin->setEnabled(!initialOpenEnded);

    QObject::connect(openEndedCheck, &QCheckBox::toggled, &dialog, [=](bool checked) {
        endSpin->setEnabled(!checked);
        if (checked) {
            endSpin->setValue(startSpin->value());
        } else if (endSpin->value() <= startSpin->value()) {
            endSpin->setValue(std::min(86400.0, startSpin->value() + 60.0));
        }
    });
    QObject::connect(startSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [=](double value) {
        if (openEndedCheck->isChecked()) {
            endSpin->setValue(value);
        }
    });

    auto* severityCombo = new QComboBox(&dialog);
    severityCombo->addItem("Low", static_cast<int>(safecrowd::domain::ScenarioElementSeverity::Low));
    severityCombo->addItem("Medium", static_cast<int>(safecrowd::domain::ScenarioElementSeverity::Medium));
    severityCombo->addItem("High", static_cast<int>(safecrowd::domain::ScenarioElementSeverity::High));
    severityCombo->setCurrentIndex(std::max(0, severityCombo->findData(static_cast<int>(hazard->severity))));

    auto* radiusSpin = new QDoubleSpinBox(&dialog);
    radiusSpin->setRange(0.0, 10000.0);
    radiusSpin->setDecimals(1);
    radiusSpin->setSingleStep(0.5);
    radiusSpin->setSuffix(" m");
    radiusSpin->setValue(safecrowd::domain::environmentHazardRadiusMeters(*hazard));

    auto* noteEdit = new QPlainTextEdit(&dialog);
    noteEdit->setPlainText(QString::fromStdString(hazard->note));
    noteEdit->setMinimumHeight(72);

    form->addRow("Name", nameEdit);
    form->addRow("Affected zone", zoneCombo);
    form->addRow("X", xSpin);
    form->addRow("Y", ySpin);
    form->addRow("Start", startSpin);
    form->addRow("End", endSpin);
    form->addRow("", openEndedCheck);
    form->addRow("Severity", severityCombo);
    form->addRow("Radius", radiusSpin);
    form->addRow("Note", noteEdit);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, "Edit hazard", "Enter a hazard name.");
            nameEdit->setFocus();
            return;
        }

        const auto selectedZoneId = zoneCombo->currentData().toString().toStdString();
        const auto selectedZone = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
            return zone.id == selectedZoneId;
        });
        if (selectedZone == layout.zones.end()) {
            QMessageBox::warning(&dialog, "Edit hazard", "Select a valid affected zone.");
            zoneCombo->setFocus();
            return;
        }

        const safecrowd::domain::Point2D selectedPosition{
            .x = xSpin->value(),
            .y = ySpin->value(),
        };
        if (!pointInPolygon(selectedZone->area, selectedPosition)) {
            QMessageBox::warning(&dialog, "Edit hazard", "The hazard location must stay inside the affected zone.");
            xSpin->setFocus();
            return;
        }
        if (!openEndedCheck->isChecked() && endSpin->value() <= startSpin->value()) {
            QMessageBox::warning(&dialog, "Edit hazard", "Set the end time after the start time.");
            endSpin->setFocus();
            return;
        }

        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const auto name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        return false;
    }

    const auto selectedZoneId = zoneCombo->currentData().toString().toStdString();
    const auto selectedZone = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == selectedZoneId;
    });
    if (selectedZone == layout.zones.end()) {
        return false;
    }

    const safecrowd::domain::Point2D selectedPosition{
        .x = xSpin->value(),
        .y = ySpin->value(),
    };
    if (!pointInPolygon(selectedZone->area, selectedPosition)) {
        QMessageBox::warning(parent, "Edit hazard", "The hazard location must stay inside the affected zone.");
        return false;
    }
    if (!openEndedCheck->isChecked() && endSpin->value() <= startSpin->value()) {
        QMessageBox::warning(parent, "Edit hazard", "Set the end time after the start time.");
        return false;
    }

    hazard->name = name.toStdString();
    hazard->affectedZoneId = selectedZoneId;
    hazard->floorId = selectedZone->floorId;
    hazard->position = selectedPosition;
    hazard->startSeconds = startSpin->value();
    hazard->endSeconds = openEndedCheck->isChecked() ? hazard->startSeconds : endSpin->value();
    hazard->severity = static_cast<safecrowd::domain::ScenarioElementSeverity>(severityCombo->currentData().toInt());
    hazard->radiusMeters = std::max(0.0, radiusSpin->value());
    hazard->note = noteEdit->toPlainText().trimmed().toStdString();
    return true;
}

int draftOccupantCount(const safecrowd::domain::ScenarioDraft& scenario) {
    int total = 0;
    for (const auto& placement : scenario.population.initialPlacements) {
        total += static_cast<int>(placement.targetAgentCount);
    }
    for (const auto& source : scenario.population.occupantSources) {
        total += static_cast<int>(source.targetAgentCount);
    }
    return total;
}

double normalizedRunTimeLimitSeconds(const safecrowd::domain::ExecutionConfig& execution) {
    return execution.timeLimitSeconds > 0.0 ? execution.timeLimitSeconds : kDefaultRunTimeLimitSeconds;
}

double normalizedRunSampleIntervalSeconds(const safecrowd::domain::ExecutionConfig& execution) {
    return execution.sampleIntervalSeconds > 0.0 ? execution.sampleIntervalSeconds : kDefaultRunSampleIntervalSeconds;
}

int normalizedRunRepeatCount(const safecrowd::domain::ExecutionConfig& execution) {
    const auto repeatCount = std::min<std::uint32_t>(
        execution.repeatCount,
        safecrowd::domain::kScenarioExecutionMaxRepeatCount);
    return std::max(1, static_cast<int>(repeatCount));
}

int normalizedRunSeed(const safecrowd::domain::ExecutionConfig& execution) {
    if (execution.baseSeed == 0) {
        return 1;
    }
    const auto seed = std::min<std::uint32_t>(
        execution.baseSeed,
        static_cast<std::uint32_t>(kMaxUiSeed));
    return std::max(1, static_cast<int>(seed));
}

QString wayfindingModeLabel(safecrowd::domain::ScenarioWayfindingMode mode) {
    switch (mode) {
    case safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding:
        return "Local wayfinding";
    case safecrowd::domain::ScenarioWayfindingMode::FullKnowledge:
    default:
        return "Full knowledge";
    }
}

safecrowd::domain::ScenarioWayfindingMode initialRunWayfindingMode(
    const std::vector<ScenarioAuthoringWidget::ScenarioState>& scenarios,
    const std::vector<std::size_t>& stagedIndexes) {
    for (const auto scenarioIndex : stagedIndexes) {
        if (scenarioIndex < scenarios.size()) {
            return scenarios[scenarioIndex].draft.execution.wayfindingMode;
        }
    }
    return safecrowd::domain::ScenarioWayfindingMode::FullKnowledge;
}

safecrowd::domain::ScenarioWayfindingMode wayfindingModeFromCombo(const QComboBox* combo) {
    if (combo == nullptr) {
        return safecrowd::domain::ScenarioWayfindingMode::FullKnowledge;
    }
    bool ok = false;
    const auto raw = combo->currentData().toInt(&ok);
    if (!ok) {
        return safecrowd::domain::ScenarioWayfindingMode::FullKnowledge;
    }
    if (raw == static_cast<int>(safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding)) {
        return safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding;
    }
    return safecrowd::domain::ScenarioWayfindingMode::FullKnowledge;
}

QString signedDelta(int delta) {
    return delta > 0 ? QString("+%1").arg(delta) : QString::number(delta);
}

QString countChangeSummary(const QString& label, int baseline, int variant) {
    const int delta = variant - baseline;
    if (delta == 0) {
        return QString("%1 details changed").arg(label);
    }
    return QString("%1 %2 (%3 -> %4)").arg(label, signedDelta(delta)).arg(baseline).arg(variant);
}

QString boolValue(bool value) {
    return value ? "on" : "off";
}

QString buildChangeSummaryLine(
    const safecrowd::domain::ScenarioDraft& baseline,
    const safecrowd::domain::ScenarioDraft& variant,
    const std::string& key) {
    if (key == "population.placements") {
        const auto baselinePlacements = static_cast<int>(
            baseline.population.initialPlacements.size() + baseline.population.occupantSources.size());
        const auto variantPlacements = static_cast<int>(
            variant.population.initialPlacements.size() + variant.population.occupantSources.size());
        QStringList parts;
        const int occupantDelta = draftOccupantCount(variant) - draftOccupantCount(baseline);
        if (occupantDelta != 0) {
            parts << QString("%1 occupants").arg(signedDelta(occupantDelta));
        }
        if (baselinePlacements != variantPlacements) {
            parts << countChangeSummary("placements", baselinePlacements, variantPlacements);
        }
        if (parts.isEmpty()) {
            parts << "placement details changed";
        }
        return QString("population.placements (%1)").arg(parts.join(", "));
    }
    if (key == "environment.reducedVisibility") {
        return QString("environment.reducedVisibility (%1 -> %2)")
            .arg(boolValue(baseline.environment.reducedVisibility), boolValue(variant.environment.reducedVisibility));
    }
    if (key == "environment.familiarityProfile") {
        return QString("environment.familiarityProfile (%1 -> %2)")
            .arg(QString::fromStdString(baseline.environment.familiarityProfile),
                 QString::fromStdString(variant.environment.familiarityProfile));
    }
    if (key == "environment.guidanceProfile") {
        return QString("environment.guidanceProfile (%1 -> %2)")
            .arg(QString::fromStdString(baseline.environment.guidanceProfile),
                 QString::fromStdString(variant.environment.guidanceProfile));
    }
    if (key == "environment.hazards") {
        auto summary = countChangeSummary(
            "hazards",
            static_cast<int>(baseline.environment.hazards.size()),
            static_cast<int>(variant.environment.hazards.size()));
        if (hasSmokeHazard(variant.environment)) {
            summary += ", smoke linked to reduced visibility concept";
        }
        return QString("environment.hazards (%1)").arg(summary);
    }
    if (key == "control.events") {
        return QString("control.events (%1)")
            .arg(countChangeSummary("events", static_cast<int>(baseline.control.events.size()),
                                    static_cast<int>(variant.control.events.size())));
    }
    if (key == "control.connectionBlocks") {
        return QString("control.connectionBlocks (%1)")
            .arg(countChangeSummary("blocks", static_cast<int>(baseline.control.connectionBlocks.size()),
                                    static_cast<int>(variant.control.connectionBlocks.size())));
    }
    if (key == "control.routeGuidances") {
        return QString("control.routeGuidances (%1)")
            .arg(countChangeSummary("guidance", static_cast<int>(baseline.control.routeGuidances.size()),
                                    static_cast<int>(variant.control.routeGuidances.size())));
    }
    if (key == "execution.timeLimit") {
        return QString("execution.timeLimit (%1s -> %2s)")
            .arg(baseline.execution.timeLimitSeconds, 0, 'f', 1)
            .arg(variant.execution.timeLimitSeconds, 0, 'f', 1);
    }
    if (key == "execution.sampleInterval") {
        return QString("execution.sampleInterval (%1s -> %2s)")
            .arg(baseline.execution.sampleIntervalSeconds, 0, 'f', 1)
            .arg(variant.execution.sampleIntervalSeconds, 0, 'f', 1);
    }
    if (key == "execution.repeatCount") {
        return QString("execution.repeatCount (%1 -> %2)")
            .arg(baseline.execution.repeatCount)
            .arg(variant.execution.repeatCount);
    }
    if (key == "execution.baseSeed") {
        return QString("execution.baseSeed (%1 -> %2)")
            .arg(baseline.execution.baseSeed)
            .arg(variant.execution.baseSeed);
    }
    if (key == "execution.recordOccupantHistory") {
        return QString("execution.recordOccupantHistory (%1 -> %2)")
            .arg(boolValue(baseline.execution.recordOccupantHistory),
                 boolValue(variant.execution.recordOccupantHistory));
    }
    if (key == "execution.wayfindingMode") {
        return QString("execution.wayfindingMode (%1 -> %2)")
            .arg(wayfindingModeLabel(baseline.execution.wayfindingMode),
                 wayfindingModeLabel(variant.execution.wayfindingMode));
    }
    return QString::fromStdString(key);
}

QString changeCategoryLabel(const std::string& key) {
    if (key.rfind("population.", 0) == 0) {
        return "Occupant";
    }
    if (key == "environment.hazards") {
        return "Hazards";
    }
    if (key.rfind("environment.", 0) == 0) {
        return "Layout";
    }
    if (key.rfind("control.", 0) == 0) {
        return "Events";
    }
    if (key.rfind("execution.", 0) == 0) {
        return "Run";
    }
    return "Change";
}

QString compactChangeSummary(const QString& summary) {
    auto compact = summary;
    compact.replace("population.placements", "crowd placements");
    compact.replace("environment.reducedVisibility", "layout visibility");
    compact.replace("environment.familiarityProfile", "layout familiarity");
    compact.replace("environment.guidanceProfile", "layout guidance");
    compact.replace("environment.hazards", "hazards");
    compact.replace("control.events", "events");
    compact.replace("control.connectionBlocks", "blocked events");
    compact.replace("control.routeGuidances", "route guidance");
    compact.replace("execution.timeLimit", "run time limit");
    compact.replace("execution.sampleInterval", "run sample interval");
    compact.replace("execution.repeatCount", "run repeat count");
    compact.replace("execution.baseSeed", "run base seed");
    compact.replace("execution.recordOccupantHistory", "run occupant history");
    compact.replace("execution.wayfindingMode", "run wayfinding");
    return compact;
}

QStringList buildChangeSummaryLines(
    const safecrowd::domain::ScenarioDraft& baseline,
    const safecrowd::domain::ScenarioDraft& variant) {
    QStringList changes;
    for (const auto& key : variant.variationDiffKeys) {
        changes << buildChangeSummaryLine(baseline, variant, key);
    }
    return changes;
}

void clearLayout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }
    while (auto* item = layout->takeAt(0)) {
        if (auto* childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        if (auto* widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
}

QFrame* createInspectorCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(
        "QFrame { background: #ffffff; border: 1px solid #d7e0ea; border-radius: 12px; }"
        "QLabel { background: transparent; border: 0; }");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    card->setMinimumWidth(0);
    return card;
}

QLabel* createRoleBadge(const QString& text, bool alternative, QWidget* parent) {
    auto* badge = createLabel(text, parent, ui::FontRole::Caption);
    badge->setAlignment(Qt::AlignCenter);
    badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    badge->setStyleSheet(alternative
        ? "QLabel { background: #fff7ed; border: 1px solid #fed7aa; border-radius: 9px; color: #9a3412; padding: 3px 8px; }"
        : "QLabel { background: #e6eef8; border: 1px solid #b8c6d6; border-radius: 9px; color: #1f5fae; padding: 3px 8px; }");
    return badge;
}

QString scenarioRoleLabel(safecrowd::domain::ScenarioRole role) {
    switch (role) {
    case safecrowd::domain::ScenarioRole::Baseline:
        return "Baseline";
    case safecrowd::domain::ScenarioRole::Recommended:
        return "Recommended";
    case safecrowd::domain::ScenarioRole::Alternative:
    default:
        return "Alternative";
    }
}

bool scenarioRoleHasBaselineDiff(safecrowd::domain::ScenarioRole role) {
    return role == safecrowd::domain::ScenarioRole::Alternative
        || role == safecrowd::domain::ScenarioRole::Recommended;
}

struct RunSettingsControls {
    std::size_t scenarioIndex{0};
    QDoubleSpinBox* timeLimitSpin{nullptr};
    QDoubleSpinBox* sampleIntervalSpin{nullptr};
    QSpinBox* repeatCountSpin{nullptr};
    QSpinBox* baseSeedSpin{nullptr};
};

bool editRunSettingsForStagedScenarios(
    std::vector<ScenarioAuthoringWidget::ScenarioState>* scenarios,
    const std::vector<std::size_t>& stagedIndexes,
    QWidget* parent) {
    if (scenarios == nullptr || stagedIndexes.empty()) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Run Settings");
    dialog.setMinimumWidth(560);
    dialog.resize(620, std::min(760, 280 + static_cast<int>(stagedIndexes.size()) * 170));
    dialog.setStyleSheet(
        "QDialog { background: #f4f7fb; }"
        "QScrollArea { background: transparent; border: 0; }"
        "QDoubleSpinBox, QSpinBox, QComboBox {"
        " background: #ffffff;"
        " border: 1px solid #c9d5e2;"
        " border-radius: 10px;"
        " padding: 6px 8px;"
        " min-height: 24px;"
        "}"
        "QComboBox::drop-down { border: 0; width: 24px; }");

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* title = createLabel("Run Settings", &dialog, ui::FontRole::Title);
    root->addWidget(title);

    auto* scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    std::vector<RunSettingsControls> controls;
    controls.reserve(stagedIndexes.size());

    auto* overallCard = createInspectorCard(content);
    auto* overallLayout = new QVBoxLayout(overallCard);
    overallLayout->setContentsMargins(12, 12, 12, 12);
    overallLayout->setSpacing(10);
    overallLayout->addWidget(createLabel("Overall", overallCard, ui::FontRole::SectionTitle));

    auto* overallForm = new QFormLayout();
    overallForm->setContentsMargins(0, 0, 0, 0);
    overallForm->setHorizontalSpacing(10);
    overallForm->setVerticalSpacing(8);

    auto* wayfindingModeCombo = new QComboBox(overallCard);
    wayfindingModeCombo->addItem(
        wayfindingModeLabel(safecrowd::domain::ScenarioWayfindingMode::FullKnowledge),
        static_cast<int>(safecrowd::domain::ScenarioWayfindingMode::FullKnowledge));
    wayfindingModeCombo->addItem(
        wayfindingModeLabel(safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding),
        static_cast<int>(safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding));
    wayfindingModeCombo->setToolTip("Wayfinding mode");
    wayfindingModeCombo->setCurrentIndex(std::max(
        0,
        wayfindingModeCombo->findData(static_cast<int>(initialRunWayfindingMode(*scenarios, stagedIndexes)))));

    overallForm->addRow("Wayfinding", wayfindingModeCombo);
    overallLayout->addLayout(overallForm);
    contentLayout->addWidget(overallCard);

    for (const auto scenarioIndex : stagedIndexes) {
        if (scenarioIndex >= scenarios->size()) {
            continue;
        }
        const auto& scenario = (*scenarios)[scenarioIndex];
        const auto& execution = scenario.draft.execution;

        auto* card = createInspectorCard(content);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(10);

        auto* headingRow = new QWidget(card);
        auto* headingLayout = new QHBoxLayout(headingRow);
        headingLayout->setContentsMargins(0, 0, 0, 0);
        headingLayout->setSpacing(8);

        auto* nameLabel = createLabel(QString::fromStdString(scenario.draft.name), headingRow, ui::FontRole::SectionTitle);
        nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        headingLayout->addWidget(nameLabel, 1);
        headingLayout->addWidget(createRoleBadge(
            scenarioRoleLabel(scenario.draft.role),
            scenario.draft.role != safecrowd::domain::ScenarioRole::Baseline,
            headingRow));
        cardLayout->addWidget(headingRow);

        auto* meta = createLabel(
            QString("%1 occupants").arg(draftOccupantCount(scenario.draft)),
            card,
            ui::FontRole::Caption);
        meta->setStyleSheet(ui::subtleTextStyleSheet());
        cardLayout->addWidget(meta);

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(10);
        form->setVerticalSpacing(8);

        auto* timeLimitSpin = new QDoubleSpinBox(card);
        timeLimitSpin->setRange(1.0, 86400.0);
        timeLimitSpin->setDecimals(0);
        timeLimitSpin->setSingleStep(30.0);
        timeLimitSpin->setSuffix(" sec");
        timeLimitSpin->setToolTip("Time limit");
        timeLimitSpin->setValue(normalizedRunTimeLimitSeconds(execution));

        auto* sampleIntervalSpin = new QDoubleSpinBox(card);
        sampleIntervalSpin->setRange(0.1, 60.0);
        sampleIntervalSpin->setDecimals(1);
        sampleIntervalSpin->setSingleStep(0.5);
        sampleIntervalSpin->setSuffix(" sec");
        sampleIntervalSpin->setToolTip("Sample interval");
        sampleIntervalSpin->setValue(normalizedRunSampleIntervalSeconds(execution));

        auto* repeatCountSpin = new QSpinBox(card);
        repeatCountSpin->setRange(1, static_cast<int>(safecrowd::domain::kScenarioExecutionMaxRepeatCount));
        repeatCountSpin->setSuffix(" runs");
        repeatCountSpin->setToolTip("Repeat count");
        repeatCountSpin->setValue(normalizedRunRepeatCount(execution));

        auto* baseSeedSpin = new QSpinBox(card);
        baseSeedSpin->setRange(1, kMaxUiSeed);
        baseSeedSpin->setToolTip("Base random seed");
        baseSeedSpin->setValue(normalizedRunSeed(execution));

        form->addRow("Time limit", timeLimitSpin);
        form->addRow("Sample interval", sampleIntervalSpin);
        form->addRow("Repeats", repeatCountSpin);
        form->addRow("Seed", baseSeedSpin);
        cardLayout->addLayout(form);

        controls.push_back({
            .scenarioIndex = scenarioIndex,
            .timeLimitSpin = timeLimitSpin,
            .sampleIntervalSpin = sampleIntervalSpin,
            .repeatCountSpin = repeatCountSpin,
            .baseSeedSpin = baseSeedSpin,
        });
        contentLayout->addWidget(card);
    }

    contentLayout->addStretch(1);
    scrollArea->setWidget(content);
    root->addWidget(scrollArea, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (auto* runButton = buttons->button(QDialogButtonBox::Ok); runButton != nullptr) {
        runButton->setText("Run");
        runButton->setFont(ui::font(ui::FontRole::Body));
        runButton->setStyleSheet(ui::primaryButtonStyleSheet());
    }
    if (auto* cancelButton = buttons->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setFont(ui::font(ui::FontRole::Body));
        cancelButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    }
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const auto wayfindingMode = wayfindingModeFromCombo(wayfindingModeCombo);
    for (const auto& control : controls) {
        if (control.scenarioIndex >= scenarios->size()) {
            continue;
        }
        auto& execution = (*scenarios)[control.scenarioIndex].draft.execution;
        execution.timeLimitSeconds = control.timeLimitSpin->value();
        execution.sampleIntervalSeconds = control.sampleIntervalSpin->value();
        execution.repeatCount = static_cast<std::uint32_t>(control.repeatCountSpin->value());
        execution.baseSeed = static_cast<std::uint32_t>(control.baseSeedSpin->value());
        execution.wayfindingMode = wayfindingMode;
    }
    return true;
}

void addMetaRow(QVBoxLayout* layout, const QString& label, const QString& value, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto* labelWidget = createLabel(label, row, ui::FontRole::Caption);
    labelWidget->setStyleSheet(ui::subtleTextStyleSheet());
    labelWidget->setMinimumWidth(62);
    labelWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* valueWidget = createLabel(value.isEmpty() ? "-" : value, row, ui::FontRole::Body);
    valueWidget->setStyleSheet(ui::mutedTextStyleSheet());
    valueWidget->setMinimumWidth(0);
    valueWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    rowLayout->addWidget(labelWidget);
    rowLayout->addWidget(valueWidget, 1);
    layout->addWidget(row);
}

void addStatusMessage(QVBoxLayout* layout, const QString& text, QWidget* parent) {
    auto* message = createLabel(text, parent, ui::FontRole::Body);
    message->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(message);
}

int severitySliderValue(safecrowd::domain::ScenarioElementSeverity severity) {
    switch (severity) {
    case safecrowd::domain::ScenarioElementSeverity::Low:
        return 0;
    case safecrowd::domain::ScenarioElementSeverity::High:
        return 2;
    case safecrowd::domain::ScenarioElementSeverity::Medium:
    default:
        return 1;
    }
}

safecrowd::domain::ScenarioElementSeverity severityFromSliderValue(int value) {
    if (value <= 0) {
        return safecrowd::domain::ScenarioElementSeverity::Low;
    }
    if (value >= 2) {
        return safecrowd::domain::ScenarioElementSeverity::High;
    }
    return safecrowd::domain::ScenarioElementSeverity::Medium;
}

QString intervalsToEditorText(const std::vector<safecrowd::domain::ConnectionBlockIntervalDraft>& intervals) {
    QStringList lines;
    for (const auto& interval : intervals) {
        lines << QString("%1 - %2")
            .arg(std::max(0.0, interval.startSeconds), 0, 'f', 1)
            .arg(std::max(0.0, interval.endSeconds), 0, 'f', 1);
    }
    return lines.join('\n');
}

bool parseIntervalEditorText(
    const QString& text,
    std::vector<safecrowd::domain::ConnectionBlockIntervalDraft>* intervals,
    QString* errorMessage) {
    if (intervals == nullptr) {
        return false;
    }

    std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> parsed;
    const auto lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const auto line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QString startText;
        QString endText;
        const auto dashIndex = line.indexOf('-');
        if (dashIndex >= 0) {
            startText = line.left(dashIndex).trimmed();
            endText = line.mid(dashIndex + 1).trimmed();
        } else {
            const auto parts = line.simplified().split(' ');
            if (parts.size() == 2) {
                startText = parts.at(0);
                endText = parts.at(1);
            }
        }

        bool startOk = false;
        bool endOk = false;
        const auto startSeconds = startText.toDouble(&startOk);
        const auto endSeconds = endText.toDouble(&endOk);
        if (!startOk || !endOk || startSeconds < 0.0 || endSeconds < startSeconds) {
            if (errorMessage != nullptr) {
                *errorMessage = QString("Use \"start - end\" seconds on line %1. End must be greater than or equal to start.").arg(i + 1);
            }
            return false;
        }

        parsed.push_back({
            .startSeconds = startSeconds,
            .endSeconds = endSeconds,
        });
    }

    *intervals = std::move(parsed);
    return true;
}

QString routeGuidancePeriodsToEditorText(const std::vector<safecrowd::domain::RouteGuidancePeriodDraft>& periods) {
    std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals;
    intervals.reserve(periods.size());
    for (const auto& period : periods) {
        intervals.push_back({
            .startSeconds = period.startSeconds,
            .endSeconds = period.endSeconds,
        });
    }
    return intervalsToEditorText(intervals);
}

std::vector<safecrowd::domain::RouteGuidancePeriodDraft> routeGuidancePeriodsFromIntervals(
    const std::vector<safecrowd::domain::ConnectionBlockIntervalDraft>& intervals) {
    std::vector<safecrowd::domain::RouteGuidancePeriodDraft> periods;
    periods.reserve(intervals.size());
    for (const auto& interval : intervals) {
        periods.push_back({
            .startSeconds = interval.startSeconds,
            .endSeconds = interval.endSeconds,
        });
    }
    return periods;
}

QWidget* createSliderEditor(
    QWidget* parent,
    QSlider** sliderOut,
    QLabel** valueLabelOut,
    int minimum,
    int maximum,
    int value,
    std::function<QString(int)> valueText) {
    auto* container = new QWidget(parent);
    container->setMinimumWidth(0);
    container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(minimum, maximum);
    slider->setValue(std::clamp(value, minimum, maximum));
    slider->setMinimumWidth(0);
    slider->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(slider, 1);

    auto* label = createLabel(valueText(slider->value()), container, ui::FontRole::Caption);
    label->setMinimumWidth(42);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(label);

    QObject::connect(slider, &QSlider::valueChanged, label, [label, valueText = std::move(valueText)](int changedValue) {
        label->setText(valueText(changedValue));
    });

    if (sliderOut != nullptr) {
        *sliderOut = slider;
    }
    if (valueLabelOut != nullptr) {
        *valueLabelOut = label;
    }
    return container;
}

void configureInspectorForm(QFormLayout* form) {
    if (form == nullptr) {
        return;
    }

    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
}

void constrainInspectorField(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }

    widget->setMinimumWidth(0);
    widget->setSizePolicy(QSizePolicy::Ignored, widget->sizePolicy().verticalPolicy());
}

void configureInspectorCombo(QComboBox* combo) {
    if (combo == nullptr) {
        return;
    }

    combo->setMinimumWidth(0);
    combo->setMinimumContentsLength(8);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    combo->setToolTip(combo->currentText());
    QObject::connect(combo, &QComboBox::currentTextChanged, combo, [combo](const QString& text) {
        combo->setToolTip(text);
    });
}

void configureInspectorTextEdit(QPlainTextEdit* edit) {
    if (edit == nullptr) {
        return;
    }

    edit->setMinimumWidth(0);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

void configureInspectorActionButton(QPushButton* button) {
    if (button == nullptr) {
        return;
    }

    button->setMinimumWidth(0);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

QComboBox* createZoneCombo(
    QWidget* parent,
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& selectedZoneId) {
    auto* combo = new QComboBox(parent);
    for (const auto& zone : layout.zones) {
        combo->addItem(zoneLabel(zone), QString::fromStdString(zone.id));
    }
    combo->setCurrentIndex(std::max(0, combo->findData(QString::fromStdString(selectedZoneId))));
    configureInspectorCombo(combo);
    return combo;
}

QComboBox* createExitZoneCombo(
    QWidget* parent,
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& selectedZoneId) {
    auto* combo = new QComboBox(parent);
    combo->addItem("Nearest exit", QString{});
    for (const auto& zone : layout.zones) {
        if (zone.kind == safecrowd::domain::ZoneKind::Exit) {
            combo->addItem(zoneLabel(zone), QString::fromStdString(zone.id));
        }
    }
    combo->setCurrentIndex(std::max(0, combo->findData(QString::fromStdString(selectedZoneId))));
    configureInspectorCombo(combo);
    return combo;
}

QComboBox* createConnectionCombo(
    QWidget* parent,
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& selectedConnectionId,
    bool includeNone = false,
    const QString& noneLabel = QStringLiteral("Use zone position")) {
    auto* combo = new QComboBox(parent);
    if (includeNone) {
        combo->addItem(noneLabel, QString{});
    }
    for (const auto& connection : layout.connections) {
        combo->addItem(connectionLabel(layout, connection), QString::fromStdString(connection.id));
    }
    combo->setCurrentIndex(std::max(0, combo->findData(QString::fromStdString(selectedConnectionId))));
    configureInspectorCombo(combo);
    return combo;
}

const safecrowd::domain::Zone2D* findZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& zoneId) {
    const auto zoneIdStd = zoneId.toStdString();
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneIdStd;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

const safecrowd::domain::Zone2D* findZoneContainingPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& point) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return pointInPolygon(zone.area, point);
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

const safecrowd::domain::Connection2D* findConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& connectionId) {
    const auto connectionIdStd = connectionId.toStdString();
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return connection.id == connectionIdStd;
    });
    return it == layout.connections.end() ? nullptr : &(*it);
}

safecrowd::domain::Point2D connectionCenter(const safecrowd::domain::Connection2D& connection) {
    return {
        .x = (connection.centerSpan.start.x + connection.centerSpan.end.x) * 0.5,
        .y = (connection.centerSpan.start.y + connection.centerSpan.end.y) * 0.5,
    };
}

void addDiffRow(QVBoxLayout* layout, const QString& category, const QString& summary, QWidget* parent) {
    auto* row = new QFrame(parent);
    row->setStyleSheet(
        "QFrame { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 10px; }"
        "QLabel { background: transparent; border: 0; }");
    auto* rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(7, 6, 7, 7);
    rowLayout->setSpacing(5);

    auto* categoryBadge = createLabel(category, row, ui::FontRole::Caption);
    categoryBadge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    categoryBadge->setStyleSheet(
        "QLabel { background: #e6eef8; border: 1px solid #c9d5e2; border-radius: 8px; color: #1f5fae; padding: 3px 7px; }");
    categoryBadge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    auto* summaryLabel = createLabel(summary, row, ui::FontRole::Caption);
    summaryLabel->setStyleSheet(ui::mutedTextStyleSheet());
    summaryLabel->setMinimumWidth(0);
    summaryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    rowLayout->addWidget(categoryBadge);
    rowLayout->addWidget(summaryLabel);
    layout->addWidget(row);
}

int totalOccupantCount(const ScenarioAuthoringWidget::ScenarioState& scenario) {
    int total = 0;
    for (const auto& placement : scenario.crowdPlacements) {
        total += std::max(0, placement.occupantCount);
    }
    if (total > 0 || !scenario.crowdPlacements.empty()) {
        return total;
    }

    for (const auto& placement : scenario.draft.population.initialPlacements) {
        total += static_cast<int>(placement.targetAgentCount);
    }
    for (const auto& source : scenario.draft.population.occupantSources) {
        total += static_cast<int>(source.targetAgentCount);
    }
    return total;
}

bool scenarioHasOccupants(const ScenarioAuthoringWidget::ScenarioState& scenario) {
    return totalOccupantCount(scenario) > 0;
}

const safecrowd::domain::Zone2D* firstStartZone(const safecrowd::domain::FacilityLayout2D& layout) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Room || zone.kind == safecrowd::domain::ZoneKind::Unknown;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

const safecrowd::domain::Zone2D* firstDestinationZone(const safecrowd::domain::FacilityLayout2D& layout) {
    const auto exitIt = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Exit;
    });
    if (exitIt != layout.zones.end()) {
        return &(*exitIt);
    }
    return layout.zones.empty() ? nullptr : &layout.zones.back();
}

QIcon scenarioNavigationIcon(const QString& resourcePath, const QColor& color) {
    return makeSvgToolIcon(resourcePath, color, QSize(22, 22));
}

QIcon crowdTreeIcon(const QString& resourcePath, const QColor& color) {
    return makeSvgToolIcon(resourcePath, color, QSize(18, 18));
}

QIcon individualCrowdTreeIcon() {
    return crowdTreeIcon(
        QStringLiteral(":/tool-icons/scenario-authoring/individual-occupant.svg"),
        QColor("#1f5fae"));
}

QIcon groupCrowdTreeIcon() {
    return crowdTreeIcon(
        QStringLiteral(":/tool-icons/scenario-authoring/add-occupant-group.svg"),
        QColor("#1f5fae"));
}

QIcon sourceCrowdTreeIcon() {
    return crowdTreeIcon(QStringLiteral(":/tool-icons/scenario-authoring/source.svg"), QColor("#1f5fae"));
}

std::vector<NavigationTreeNode> buildCrowdTree(const ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr || scenario->crowdPlacements.empty()) {
        return {};
    }

    std::vector<NavigationTreeNode> placements;
    for (const auto& placement : scenario->crowdPlacements) {
        const bool group = placement.kind == ScenarioCrowdPlacementKind::Group;
        const bool source = placement.kind == ScenarioCrowdPlacementKind::Source;
        std::vector<NavigationTreeNode> occupants;
        for (int index = 1; group && index <= placement.occupantCount; ++index) {
            occupants.push_back({
                .label = QString("Occupant %1").arg(index),
                .id = QString("%1/occupant-%2").arg(placement.id).arg(index),
                .detail = QString("Floor: %1\nZone: %2\nVelocity: (%3, %4)")
                              .arg(placement.floorId)
                              .arg(placement.zoneId)
                              .arg(placement.velocity.x, 0, 'f', 2)
                              .arg(placement.velocity.y, 0, 'f', 2),
                .icon = individualCrowdTreeIcon(),
            });
        }

        const auto detail = source
            ? QString("Source schedule: %1 people every %2s for %3 min\nVelocity: (%4, %5)")
                  .arg(placement.sourceAgentsPerSpawn)
                  .arg(placement.sourceIntervalSeconds, 0, 'f', 1)
                  .arg(std::max(0.0, placement.sourceEndSeconds - placement.sourceStartSeconds) / 60.0, 0, 'f', 1)
                  .arg(placement.velocity.x, 0, 'f', 2)
                  .arg(placement.velocity.y, 0, 'f', 2)
            : QString("Velocity: (%1, %2)")
                  .arg(placement.velocity.x, 0, 'f', 2)
                  .arg(placement.velocity.y, 0, 'f', 2);
        placements.push_back({
            .label = QString("%1  -  %2  -  %3 %4")
                         .arg(
                             placement.name.isEmpty() ? placement.id : placement.name,
                             placement.zoneId)
                         .arg(placement.occupantCount)
                         .arg(placement.occupantCount == 1 ? "occupant" : "occupants"),
            .id = placement.id,
            .detail = detail,
            .icon = source ? sourceCrowdTreeIcon() : (group ? groupCrowdTreeIcon() : individualCrowdTreeIcon()),
            .children = group ? std::move(occupants) : std::vector<NavigationTreeNode>{},
            .expanded = false,
        });
    }

    return placements;
}

QWidget* createCrowdPanel(
    const ScenarioAuthoringWidget::ScenarioState* scenario,
    std::function<void(const QString&)> selectPlacementHandler,
    NavigationTreeState navigationState,
    std::function<void(const QSet<QString>&)> expandedStateChangedHandler,
    std::function<void(const QString&)> deletePlacementHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    return new NavigationTreeWidget(
        "Occupant",
        buildCrowdTree(scenario),
        "No pedestrian placements yet",
        std::move(selectPlacementHandler),
        parent,
        shell != nullptr ? shell->createPanelHeader("Occupant", parent, false) : nullptr,
        std::move(navigationState),
        std::move(expandedStateChangedHandler),
        std::move(deletePlacementHandler));
}

std::vector<NavigationTreeNode> buildEventsTree(
    const safecrowd::domain::FacilityLayout2D& layout,
    const ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr) {
        return {};
    }

    std::vector<NavigationTreeNode> sections;
    if (!scenario->events.empty()) {
        std::vector<NavigationTreeNode> events;
        for (const auto& event : scenario->events) {
            const auto eventId = QString::fromStdString(event.id);
            events.push_back({
                .label = QString::fromStdString(event.name),
                .id = eventId,
                .detail = QString::fromStdString(event.targetSummary),
                .children = {
                    {
                        .label = QString("Trigger  -  %1").arg(QString::fromStdString(event.triggerSummary)),
                        .id = QString("%1/trigger").arg(eventId),
                    },
                    {
                        .label = QString("Target  -  %1").arg(QString::fromStdString(event.targetSummary)),
                        .id = QString("%1/target").arg(eventId),
                    },
                },
                .expanded = true,
            });
        }

        sections.push_back({
            .label = QString("Operational Events (%1)").arg(static_cast<int>(scenario->events.size())),
            .children = std::move(events),
            .expanded = true,
            .selectable = false,
        });
    }

    const auto& hazards = scenario->draft.environment.hazards;
    if (!hazards.empty()) {
        std::vector<NavigationTreeNode> nodes;
        nodes.reserve(hazards.size());
        for (const auto& hazard : hazards) {
            const auto hazardId = QString::fromStdString(hazard.id);
            const auto kind = hazardKindLabel(hazard.kind);
            const auto zone = hazardZoneSummary(layout, hazard);
            const auto position = hazardPositionSummary(hazard);
            const auto schedule = hazardScheduleSummary(hazard);
            const auto severity = severityLabel(hazard.severity);
            const auto radius = safecrowd::domain::environmentHazardRadiusMeters(hazard);
            QStringList details;
            details << QString("Zone: %1").arg(zone)
                    << QString("Location: %1").arg(position)
                    << QString("Period: %1").arg(schedule)
                    << QString("Severity: %1").arg(severity)
                    << QString("Radius: %1m").arg(radius, 0, 'f', 1);
            if (hazard.kind == safecrowd::domain::EnvironmentHazardKind::Smoke) {
                details << "Visibility: reduced visibility concept";
            }

            std::vector<NavigationTreeNode> children{
                {
                    .label = QString("Kind  -  %1").arg(kind),
                    .id = QString("%1/kind").arg(hazardId),
                },
                {
                    .label = QString("Zone  -  %1").arg(zone),
                    .id = QString("%1/zone").arg(hazardId),
                },
                {
                    .label = QString("Location  -  %1").arg(position),
                    .id = QString("%1/location").arg(hazardId),
                },
                {
                    .label = QString("Period  -  %1").arg(schedule),
                    .id = QString("%1/period").arg(hazardId),
                },
                {
                    .label = QString("Severity  -  %1").arg(severity),
                    .id = QString("%1/severity").arg(hazardId),
                },
                {
                    .label = QString("Radius  -  %1m").arg(radius, 0, 'f', 1),
                    .id = QString("%1/radius").arg(hazardId),
                },
            };
            if (!hazard.note.empty()) {
                children.push_back({
                    .label = QString("Note  -  %1").arg(QString::fromStdString(hazard.note)),
                    .id = QString("%1/note").arg(hazardId),
                });
            }

            nodes.push_back({
                .label = QString("Hazard  -  %1: %2").arg(kind, QString::fromStdString(hazard.name)),
                .id = hazardId,
                .detail = details.join(" / "),
                .children = std::move(children),
                .expanded = true,
            });
        }

        sections.push_back({
            .label = QString("Hazards (%1)").arg(static_cast<int>(hazards.size())),
            .children = std::move(nodes),
            .expanded = true,
            .selectable = false,
        });
    }

    const auto& routeGuidances = scenario->draft.control.routeGuidances;
    if (!routeGuidances.empty()) {
        std::vector<NavigationTreeNode> nodes;
        nodes.reserve(routeGuidances.size());
        for (const auto& guidance : routeGuidances) {
            const auto guidanceId = QString::fromStdString(guidance.id);
            const auto installLabel = routeGuidanceInstallLabel(layout, guidance);
            const auto exitLabel = guidance.guidedExitZoneId.empty()
                ? QStringLiteral("Nearest exit")
                : zoneName(layout, guidance.guidedExitZoneId);
            const auto locationLabel = routeGuidancePositionSummary(guidance);
            QString periodSummary = QStringLiteral("Always");
            if (!guidance.periods.empty()) {
                periodSummary = blockScheduleSummary(safecrowd::domain::ConnectionBlockDraft{
                    .intervals = [&]() {
                        std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals;
                        intervals.reserve(guidance.periods.size());
                        for (const auto& period : guidance.periods) {
                            intervals.push_back({
                                .startSeconds = std::max(0.0, period.startSeconds),
                                .endSeconds = std::max(0.0, period.endSeconds),
                            });
                        }
                        return intervals;
                    }(),
                });
            }

            std::vector<NavigationTreeNode> children;
            children.reserve(4u);
            children.push_back({
                .label = QString("Exit  -  %1").arg(exitLabel),
                .id = QString("%1/exit").arg(guidanceId),
            });
            children.push_back({
                .label = QString("Install  -  %1").arg(installLabel),
                .id = QString("%1/install").arg(guidanceId),
            });
            children.push_back({
                .label = QString("Location  -  %1").arg(locationLabel),
                .id = QString("%1/location").arg(guidanceId),
            });
            children.push_back({
                .label = QString("Period  -  %1").arg(periodSummary),
                .id = QString("%1/period").arg(guidanceId),
            });

            nodes.push_back({
                .label = QString("Guidance  -  %1").arg(installLabel),
                .id = guidanceId,
                .detail = QString("Location: %1 / Period: %2").arg(locationLabel, periodSummary),
                .children = std::move(children),
                .expanded = true,
            });
        }

        sections.push_back({
            .label = QString("Route Guidance (%1)").arg(static_cast<int>(routeGuidances.size())),
            .children = std::move(nodes),
            .expanded = true,
            .selectable = false,
        });
    }

    const auto& connectionBlocks = scenario->draft.control.connectionBlocks;
    if (!connectionBlocks.empty()) {
        std::vector<NavigationTreeNode> blocks;
        for (const auto& block : connectionBlocks) {
            const auto blockId = QString::fromStdString(block.id);
            const auto targetLabel = connectionLabelForId(layout, block.connectionId);
            const auto schedule = blockScheduleSummary(block);
            blocks.push_back({
                .label = QString("Blocked  -  %1").arg(targetLabel),
                .id = blockId,
                .detail = schedule,
                .children = {
                    {
                        .label = QString("Target  -  %1").arg(targetLabel),
                        .id = QString("%1/target").arg(blockId),
                    },
                    {
                        .label = QString("Schedule  -  %1").arg(schedule),
                        .id = QString("%1/schedule").arg(blockId),
                    },
                },
                .expanded = true,
            });
        }

        sections.push_back({
            .label = QString("Blocked Doors / Exits (%1)").arg(static_cast<int>(connectionBlocks.size())),
            .children = std::move(blocks),
            .expanded = true,
            .selectable = false,
        });
    }

    return sections;
}

QWidget* createEventsPanel(
    const safecrowd::domain::FacilityLayout2D& layout,
    const ScenarioAuthoringWidget::ScenarioState* scenario,
    const WorkspaceShell* shell,
    QWidget* parent,
    std::function<void(const QString&)> selectItemHandler,
    NavigationTreeState navigationState,
    std::function<void(const QSet<QString>&)> expandedStateChangedHandler,
    std::function<void(const QString&)> deleteItemHandler,
    std::function<void(const QString&)> settingsItemHandler) {
    return new NavigationTreeWidget(
        "Events / Hazards",
        buildEventsTree(layout, scenario),
        "No operational events, hazards, or blocked exits yet",
        std::move(selectItemHandler),
        parent,
        shell != nullptr ? shell->createPanelHeader("Events / Hazards", parent, false) : nullptr,
        std::move(navigationState),
        std::move(expandedStateChangedHandler),
        std::move(deleteItemHandler),
        std::move(settingsItemHandler),
        QString("Settings..."));
}

SavedNavigationView savedNavigationView(ScenarioAuthoringWidget::NavigationView view) {
    switch (view) {
    case ScenarioAuthoringWidget::NavigationView::Crowd:
        return SavedNavigationView::Crowd;
    case ScenarioAuthoringWidget::NavigationView::Events:
        return SavedNavigationView::Events;
    case ScenarioAuthoringWidget::NavigationView::Layout:
    default:
        return SavedNavigationView::Layout;
    }
}

}  // namespace

ScenarioAuthoringWidget::ScenarioAuthoringWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)) {
    initializeUi(true);
}

ScenarioAuthoringWidget::ScenarioAuthoringWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    InitialState initialState,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)),
      scenarios_(std::move(initialState.scenarios)),
      currentScenarioIndex_(initialState.currentScenarioIndex),
      navigationView_(initialState.navigationView),
      inspectorPanelVisible_(initialState.inspectorPanelVisible),
      scenarioPanelVisible_(initialState.scenarioPanelVisible) {
    for (auto& scenario : scenarios_) {
        if (!scenarioHasOccupants(scenario)) {
            scenario.stagedForRun = false;
        }
    }
    initializeUi(false);
}

void ScenarioAuthoringWidget::initializeUi(bool promptForScenario) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(WorkspaceShellOptions{
        .showReviewPanelToggle = false,
        .reviewPanelWidth = 560,
    }, this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler(backToLayoutReviewHandler_);
    shell_->setTopBarTrailingWidget(createPanelToggleBar());

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        if (!operationalEventHistoryShortcutBlockedByTextInput()) {
            undoLastScenarioAuthoringEdit();
        }
    });
    auto* redoShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Z")), this);
    connect(redoShortcut, &QShortcut::activated, this, [this]() {
        if (!operationalEventHistoryShortcutBlockedByTextInput()) {
            redoLastScenarioAuthoringEdit();
        }
    });

    refreshRightPanel();
    rootLayout->addWidget(shell_);

    refreshNavigationPanel();
    refreshCanvas();
    refreshInspector();
    if (promptForScenario) {
        QTimer::singleShot(0, this, [this]() {
            ensureInitialScenarioPrompt();
        });
    }
}

SavedScenarioAuthoringState ScenarioAuthoringWidget::currentSavedState() const {
    SavedScenarioAuthoringState state;
    state.currentScenarioIndex = currentScenarioIndex_;
    state.navigationView = savedNavigationView(navigationView_);
    state.inspectorPanelVisible = inspectorPanelVisible_;
    state.scenarioPanelVisible = scenarioPanelVisible_;
    state.rightPanelMode = (inspectorPanelVisible_ || scenarioPanelVisible_)
        ? SavedRightPanelMode::Scenario
        : SavedRightPanelMode::None;
    state.scenarios.reserve(scenarios_.size());
    for (const auto& scenario : scenarios_) {
        auto draft = scenario.draft;
        draft.control.events = scenario.events;
        state.scenarios.push_back({
            .draft = std::move(draft),
            .baseScenarioId = scenario.baseScenarioId.toStdString(),
            .stagedForRun = scenario.stagedForRun && scenarioHasOccupants(scenario),
        });
    }
    return state;
}

ScenarioAuthoringWidget::InitialState ScenarioAuthoringWidget::currentInitialState() const {
    InitialState state;
    state.currentScenarioIndex = currentScenarioIndex_;
    state.navigationView = navigationView_;
    state.inspectorPanelVisible = inspectorPanelVisible_;
    state.scenarioPanelVisible = scenarioPanelVisible_;
    state.rightPanelMode = (inspectorPanelVisible_ || scenarioPanelVisible_)
        ? RightPanelMode::Scenario
        : RightPanelMode::None;
    state.scenarios.reserve(scenarios_.size());
    for (const auto& scenario : scenarios_) {
        auto copy = scenario;
        copy.draft.control.events = copy.events;
        copy.stagedForRun = copy.stagedForRun && scenarioHasOccupants(copy);
        state.scenarios.push_back(std::move(copy));
    }
    return state;
}

bool ScenarioAuthoringWidget::undoLastScenarioAuthoringEdit() {
    auto* history = currentScenarioHistory();
    if (history == nullptr || history->undo.empty()) {
        return false;
    }

    const auto entry = history->undo.back();
    std::optional<ScenarioHistoryEntry> currentEntry;
    if (entry.kind == ScenarioHistoryEntryKind::CrowdPlacement) {
        if (const auto current = currentCrowdPlacementHistoryEntry(entry.crowdPlacement.selectedCrowdId); current.has_value()) {
            currentEntry = ScenarioHistoryEntry{
                .kind = ScenarioHistoryEntryKind::CrowdPlacement,
                .crowdPlacement = *current,
            };
        }
    } else if (const auto current = currentOperationalEventHistoryEntry(entry.operationalEvent.selectedEventId); current.has_value()) {
        currentEntry = ScenarioHistoryEntry{
            .kind = ScenarioHistoryEntryKind::OperationalEvent,
            .operationalEvent = *current,
        };
    }
    if (!currentEntry.has_value()) {
        return false;
    }

    history->undo.pop_back();
    history->redo.push_back(std::move(*currentEntry));
    if (entry.kind == ScenarioHistoryEntryKind::CrowdPlacement) {
        return restoreCrowdPlacementHistoryEntry(entry.crowdPlacement);
    }
    return restoreOperationalEventHistoryEntry(entry.operationalEvent);
}

bool ScenarioAuthoringWidget::redoLastScenarioAuthoringEdit() {
    auto* history = currentScenarioHistory();
    if (history == nullptr || history->redo.empty()) {
        return false;
    }

    const auto entry = history->redo.back();
    std::optional<ScenarioHistoryEntry> currentEntry;
    if (entry.kind == ScenarioHistoryEntryKind::CrowdPlacement) {
        if (const auto current = currentCrowdPlacementHistoryEntry(entry.crowdPlacement.selectedCrowdId); current.has_value()) {
            currentEntry = ScenarioHistoryEntry{
                .kind = ScenarioHistoryEntryKind::CrowdPlacement,
                .crowdPlacement = *current,
            };
        }
    } else if (const auto current = currentOperationalEventHistoryEntry(entry.operationalEvent.selectedEventId); current.has_value()) {
        currentEntry = ScenarioHistoryEntry{
            .kind = ScenarioHistoryEntryKind::OperationalEvent,
            .operationalEvent = *current,
        };
    }
    if (!currentEntry.has_value()) {
        return false;
    }

    history->redo.pop_back();
    history->undo.push_back(std::move(*currentEntry));
    if (entry.kind == ScenarioHistoryEntryKind::CrowdPlacement) {
        return restoreCrowdPlacementHistoryEntry(entry.crowdPlacement);
    }
    return restoreOperationalEventHistoryEntry(entry.operationalEvent);
}

bool ScenarioAuthoringWidget::restoreCrowdPlacementHistoryEntry(const CrowdPlacementHistoryEntry& entry) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return false;
    }

    scenario->crowdPlacements = entry.placements;
    synchronizeCrowdPlacements(*scenario);
    restoreCrowdPlacementSelection(entry.selectedCrowdId);
    if (canvas_ != nullptr) {
        canvas_->setPlacements(scenario->crowdPlacements);
        if (!entry.selectedCrowdId.isEmpty()) {
            canvas_->focusPlacement(entry.selectedCrowdId);
        }
    }
    refreshNavigationPanel();
    refreshInspector();
    return true;
}

ScenarioAuthoringWidget::ScenarioHistory* ScenarioAuthoringWidget::currentScenarioHistory() {
    const auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return nullptr;
    }

    const auto scenarioId = QString::fromStdString(scenario->draft.scenarioId);
    auto it = std::find_if(scenarioHistories_.begin(), scenarioHistories_.end(), [&](const auto& history) {
        return history.scenarioId == scenarioId;
    });
    if (it != scenarioHistories_.end()) {
        return &(*it);
    }

    scenarioHistories_.push_back({
        .scenarioId = scenarioId,
    });
    return &scenarioHistories_.back();
}

std::optional<ScenarioAuthoringWidget::CrowdPlacementHistoryEntry> ScenarioAuthoringWidget::currentCrowdPlacementHistoryEntry(
    const QString& selectedCrowdId) const {
    const auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return std::nullopt;
    }

    QString resolvedSelectedCrowdId = selectedCrowdId;
    if (resolvedSelectedCrowdId.isEmpty()) {
        resolvedSelectedCrowdId = inspectorSelectionKind_ == InspectorSelectionKind::Crowd
            ? inspectorSelectionId_
            : selectedCrowdElementId_.section('/', 0, 0);
    }
    if (!resolvedSelectedCrowdId.isEmpty()
        && !std::any_of(scenario->crowdPlacements.begin(), scenario->crowdPlacements.end(), [&](const auto& placement) {
            return placement.id == resolvedSelectedCrowdId.section('/', 0, 0);
        })) {
        resolvedSelectedCrowdId.clear();
    }

    return CrowdPlacementHistoryEntry{
        .placements = scenario->crowdPlacements,
        .selectedCrowdId = resolvedSelectedCrowdId.section('/', 0, 0),
    };
}

void ScenarioAuthoringWidget::pushCrowdPlacementUndoEntry(CrowdPlacementHistoryEntry entry) {
    auto* history = currentScenarioHistory();
    if (history == nullptr) {
        return;
    }

    history->undo.push_back(ScenarioHistoryEntry{
        .kind = ScenarioHistoryEntryKind::CrowdPlacement,
        .crowdPlacement = std::move(entry),
    });
    history->redo.clear();
}

void ScenarioAuthoringWidget::synchronizeCrowdPlacements(ScenarioState& scenario) {
    scenario.draft.population.initialPlacements.clear();
    scenario.draft.population.occupantSources.clear();
    for (const auto& placement : scenario.crowdPlacements) {
        if (placement.kind == ScenarioCrowdPlacementKind::Source) {
            if (placement.area.empty()) {
                continue;
            }
            safecrowd::domain::OccupantSource2D source;
            source.id = placement.id.toStdString();
            source.zoneId = placement.zoneId.toStdString();
            source.floorId = placement.floorId.toStdString();
            source.position = placement.area.front();
            source.targetAgentCount = static_cast<std::size_t>(std::max(0, placement.occupantCount));
            source.agentsPerSpawn = static_cast<std::size_t>(std::max(1, placement.sourceAgentsPerSpawn));
            source.startSeconds = std::max(0.0, placement.sourceStartSeconds);
            source.endSeconds = std::max(source.startSeconds, placement.sourceEndSeconds);
            source.spawnIntervalSeconds = std::max(0.1, placement.sourceIntervalSeconds);
            source.initialVelocity = placement.velocity;
            scenario.draft.population.occupantSources.push_back(std::move(source));
            continue;
        }

        safecrowd::domain::InitialPlacement2D initialPlacement;
        initialPlacement.id = placement.id.toStdString();
        initialPlacement.zoneId = placement.zoneId.toStdString();
        initialPlacement.floorId = placement.floorId.toStdString();
        initialPlacement.area.outline = placement.area;
        initialPlacement.targetAgentCount = static_cast<std::size_t>(placement.occupantCount);
        initialPlacement.initialVelocity = placement.velocity;
        initialPlacement.distribution = placement.distribution;
        initialPlacement.explicitPositions = placement.generatedPositions;
        scenario.draft.population.initialPlacements.push_back(std::move(initialPlacement));
    }
    if (!scenarioHasOccupants(scenario)) {
        scenario.stagedForRun = false;
    }

    recomputeDiffKeysAfterScenarioChanged(scenario);
}

void ScenarioAuthoringWidget::restoreCrowdPlacementSelection(const QString& selectedCrowdId) {
    navigationView_ = NavigationView::Crowd;
    selectedLayoutElementId_.clear();
    selectedEventElementId_.clear();

    const auto rootId = selectedCrowdId.section('/', 0, 0);
    const auto* scenario = currentScenario();
    const bool placementExists = scenario != nullptr
        && !rootId.isEmpty()
        && std::any_of(scenario->crowdPlacements.begin(), scenario->crowdPlacements.end(), [&](const auto& placement) {
            return placement.id == rootId;
        });

    if (!placementExists) {
        selectedCrowdElementId_.clear();
        inspectorSelectionKind_ = InspectorSelectionKind::None;
        inspectorSelectionId_.clear();
        return;
    }

    selectedCrowdElementId_ = rootId;
    inspectorSelectionKind_ = InspectorSelectionKind::Crowd;
    inspectorSelectionId_ = rootId;
}

std::optional<ScenarioAuthoringWidget::OperationalEventHistoryEntry> ScenarioAuthoringWidget::currentOperationalEventHistoryEntry(
    const QString& selectedEventId) const {
    const auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return std::nullopt;
    }

    QString resolvedSelectedEventId = selectedEventId;
    if (resolvedSelectedEventId.isEmpty()) {
        resolvedSelectedEventId = (inspectorSelectionKind_ == InspectorSelectionKind::OperationalEvent
                || inspectorSelectionKind_ == InspectorSelectionKind::EnvironmentHazard
                || inspectorSelectionKind_ == InspectorSelectionKind::RouteGuidance
                || inspectorSelectionKind_ == InspectorSelectionKind::ConnectionBlock)
            ? inspectorSelectionId_
            : selectedEventElementId_.section('/', 0, 0);
    }
    if (!resolvedSelectedEventId.isEmpty()
        && !std::any_of(scenario->events.begin(), scenario->events.end(), [&](const auto& event) {
            return QString::fromStdString(event.id) == resolvedSelectedEventId.section('/', 0, 0);
        })
        && !std::any_of(scenario->draft.environment.hazards.begin(), scenario->draft.environment.hazards.end(), [&](const auto& hazard) {
            return QString::fromStdString(hazard.id) == resolvedSelectedEventId.section('/', 0, 0);
        })
        && !std::any_of(scenario->draft.control.routeGuidances.begin(), scenario->draft.control.routeGuidances.end(), [&](const auto& guidance) {
            return QString::fromStdString(guidance.id) == resolvedSelectedEventId.section('/', 0, 0);
        })
        && !std::any_of(scenario->draft.control.connectionBlocks.begin(), scenario->draft.control.connectionBlocks.end(), [&](const auto& block) {
            return QString::fromStdString(block.id) == resolvedSelectedEventId.section('/', 0, 0);
        })) {
        resolvedSelectedEventId.clear();
    }

    return OperationalEventHistoryEntry{
        .events = scenario->events,
        .connectionBlocks = scenario->draft.control.connectionBlocks,
        .hazards = scenario->draft.environment.hazards,
        .routeGuidances = scenario->draft.control.routeGuidances,
        .selectedEventId = resolvedSelectedEventId.section('/', 0, 0),
    };
}

void ScenarioAuthoringWidget::pushOperationalEventUndoEntry(OperationalEventHistoryEntry entry) {
    auto* history = currentScenarioHistory();
    if (history == nullptr) {
        return;
    }

    history->undo.push_back(ScenarioHistoryEntry{
        .kind = ScenarioHistoryEntryKind::OperationalEvent,
        .operationalEvent = std::move(entry),
    });
    history->redo.clear();
}

void ScenarioAuthoringWidget::synchronizeOperationalEvents(ScenarioState& scenario) {
    scenario.draft.control.events = scenario.events;
    recomputeDiffKeysAfterScenarioChanged(scenario);
}

void ScenarioAuthoringWidget::restoreOperationalEventSelection(const QString& selectedEventId) {
    navigationView_ = NavigationView::Events;
    selectedLayoutElementId_.clear();
    selectedCrowdElementId_.clear();

    const auto rootId = selectedEventId.section('/', 0, 0);
    const auto* scenario = currentScenario();
    if (scenario == nullptr || rootId.isEmpty()) {
        setInspectorSelectionNone();
        return;
    }

    const bool eventExists = std::any_of(scenario->events.begin(), scenario->events.end(), [&](const auto& event) {
        return QString::fromStdString(event.id) == rootId;
    });
    const bool hazardExists = std::any_of(scenario->draft.environment.hazards.begin(), scenario->draft.environment.hazards.end(), [&](const auto& hazard) {
        return QString::fromStdString(hazard.id) == rootId;
    });
    const bool guidanceExists = std::any_of(scenario->draft.control.routeGuidances.begin(), scenario->draft.control.routeGuidances.end(), [&](const auto& guidance) {
        return QString::fromStdString(guidance.id) == rootId;
    });
    const bool blockExists = std::any_of(scenario->draft.control.connectionBlocks.begin(), scenario->draft.control.connectionBlocks.end(), [&](const auto& block) {
        return QString::fromStdString(block.id) == rootId;
    });

    if (!eventExists && !hazardExists && !guidanceExists && !blockExists) {
        setInspectorSelectionNone();
        return;
    }

    selectedEventElementId_ = rootId;
    inspectorSelectionId_ = rootId;
    if (eventExists) {
        inspectorSelectionKind_ = InspectorSelectionKind::OperationalEvent;
    } else if (hazardExists) {
        inspectorSelectionKind_ = InspectorSelectionKind::EnvironmentHazard;
    } else if (guidanceExists) {
        inspectorSelectionKind_ = InspectorSelectionKind::RouteGuidance;
    } else {
        inspectorSelectionKind_ = InspectorSelectionKind::ConnectionBlock;
    }
}

bool ScenarioAuthoringWidget::restoreOperationalEventHistoryEntry(const OperationalEventHistoryEntry& entry) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return false;
    }

    scenario->events = entry.events;
    scenario->draft.control.connectionBlocks = entry.connectionBlocks;
    scenario->draft.environment.hazards = entry.hazards;
    scenario->draft.control.routeGuidances = entry.routeGuidances;
    synchronizeOperationalEvents(*scenario);
    restoreOperationalEventSelection(entry.selectedEventId);
    if (canvas_ != nullptr) {
        canvas_->setConnectionBlocks(scenario->draft.control.connectionBlocks);
        canvas_->setEnvironmentHazards(scenario->draft.environment.hazards);
        canvas_->setRouteGuidances(scenario->draft.control.routeGuidances);
    }
    refreshNavigationPanel();
    refreshInspector();
    return true;
}

void ScenarioAuthoringWidget::addEventDraft(const QString& name, const QString& trigger, const QString& target) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    const auto exists = std::any_of(scenario->events.begin(), scenario->events.end(), [&](const auto& event) {
        return QString::fromStdString(event.name) == name;
    });
    if (exists) {
        return;
    }

    const auto beforeChange = currentOperationalEventHistoryEntry();
    scenario->events.push_back({
        .id = QString("event-%1").arg(static_cast<int>(scenario->events.size()) + 1).toStdString(),
        .name = name.toStdString(),
        .triggerSummary = trigger.toStdString(),
        .targetSummary = target.toStdString(),
    });
    if (beforeChange.has_value()) {
        pushOperationalEventUndoEntry(*beforeChange);
    }
    synchronizeOperationalEvents(*scenario);
    restoreOperationalEventSelection(QString::fromStdString(scenario->events.back().id));
    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::createScenarioFromCurrent() {
    showScenarioNameDialog(currentScenarioIndex_);
}

void ScenarioAuthoringWidget::createScenarioWithName(const QString& name, int sourceIndex) {
    const auto trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return;
    }

    const auto newScenarioId = QString("scenario-%1").arg(scenarios_.size() + 1).toStdString();
    ScenarioState scenario;
    if (sourceIndex >= 0 && sourceIndex < static_cast<int>(scenarios_.size())) {
        const auto& source = scenarios_[sourceIndex];
        scenario.draft = safecrowd::domain::duplicateScenarioDraft(
            source.draft, newScenarioId, trimmedName.toStdString());
        scenario.events = source.events;
        scenario.crowdPlacements = source.crowdPlacements;
        scenario.startText = source.startText;
        scenario.destinationText = source.destinationText;
        scenario.baseScenarioId = scenarioRoleHasBaselineDiff(source.draft.role)
            ? source.baseScenarioId
            : QString::fromStdString(source.draft.scenarioId);
        scenario.stagedForRun = false;
    } else {
        scenario.draft.role = safecrowd::domain::ScenarioRole::Baseline;
        scenario.draft.sourceTemplateId = "sprint1-baseline";
        scenario.draft.execution.timeLimitSeconds = 600.0;
        scenario.draft.execution.sampleIntervalSeconds = 1.0;
        scenario.draft.execution.repeatCount = 1;
        scenario.draft.execution.baseSeed = 1;
        scenario.draft.name = trimmedName.toStdString();
        scenario.draft.scenarioId = newScenarioId;

        const auto* destinationZone = firstDestinationZone(layout_);
        const auto* startZone = firstStartZone(layout_);
        if (startZone != nullptr) {
            scenario.startText = zoneLabel(*startZone);
        }
        if (destinationZone != nullptr) {
            scenario.destinationText = zoneLabel(*destinationZone);
        }
    }

    scenarios_.push_back(std::move(scenario));
    currentScenarioIndex_ = static_cast<int>(scenarios_.size()) - 1;
    selectedLayoutElementId_.clear();
    selectedCrowdElementId_.clear();
    setInspectorSelectionNone();
    recomputeVariationDiffKeysIfAlternative(scenarios_.back());
    refreshScenarioSwitcher();
    refreshCanvas();
    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::ensureInitialScenarioPrompt() {
    if (!scenarios_.empty()) {
        return;
    }

    showScenarioNameDialog(-1);
}

void ScenarioAuthoringWidget::refreshCanvas() {
    if (shell_ == nullptr || currentScenario() == nullptr) {
        showEmptyCanvas();
        return;
    }

    auto* scenario = currentScenario();
    canvas_ = new ScenarioCanvasWidget(layout_, shell_);
    canvas_->setPlacements(scenario->crowdPlacements);
    canvas_->setPlacementsChangedHandler([this](const std::vector<ScenarioCrowdPlacement>& placements) {
        updateCurrentScenarioPlacements(placements);
    });
    canvas_->setScenarioElementSelectionChangedHandler([this](const ScenarioCanvasSelection& selection) {
        setInspectorSelectionFromCanvas(selection);
    });
    canvas_->setLayoutElementActivatedHandler([this](const QString& elementId) {
        selectedLayoutElementId_ = elementId;
        if (!elementId.isEmpty()) {
            selectedCrowdElementId_.clear();
            selectedEventElementId_.clear();
            inspectorSelectionKind_ = InspectorSelectionKind::Layout;
            inspectorSelectionId_ = elementId;
            if (navigationView_ != NavigationView::Layout) {
                navigationView_ = NavigationView::Layout;
                refreshNavigationPanel();
                refreshInspector();
                return;
            }
        } else if (inspectorSelectionKind_ == InspectorSelectionKind::Layout) {
            setInspectorSelectionNone();
        }
        if (navigationView_ == NavigationView::Layout) {
            refreshNavigationPanel();
        }
        refreshInspector();
    });
    canvas_->setCrowdSelectionChangedHandler([this](const QString& elementId) {
        selectedCrowdElementId_ = elementId;
        if (!elementId.isEmpty()) {
            selectedLayoutElementId_.clear();
            selectedEventElementId_.clear();
            inspectorSelectionKind_ = InspectorSelectionKind::Crowd;
            inspectorSelectionId_ = elementId.section('/', 0, 0);
            if (navigationView_ != NavigationView::Crowd) {
                navigationView_ = NavigationView::Crowd;
                refreshNavigationPanel();
                refreshInspector();
                return;
            }
        } else if (inspectorSelectionKind_ == InspectorSelectionKind::Crowd) {
            setInspectorSelectionNone();
        }
        if (navigationView_ == NavigationView::Crowd) {
            refreshNavigationPanel();
        }
        refreshInspector();
    });
    canvas_->setConnectionBlocks(scenario->draft.control.connectionBlocks);
    canvas_->setConnectionBlocksChangedHandler([this](const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        if (connectionBlocksEqual(current->draft.control.connectionBlocks, blocks)) {
            return;
        }
        const auto beforeChange = currentOperationalEventHistoryEntry(selectedEventElementId_);
        current->draft.control.connectionBlocks = blocks;
        if (beforeChange.has_value()) {
            pushOperationalEventUndoEntry(*beforeChange);
        }
        recomputeDiffKeysAfterScenarioChanged(*current);
        restoreOperationalEventSelection(selectedEventElementId_);
        refreshNavigationPanel();
        refreshInspector();
    });
    canvas_->setEnvironmentHazards(scenario->draft.environment.hazards);
    canvas_->setEnvironmentHazardsChangedHandler([this](const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        if (environmentHazardsEqual(current->draft.environment.hazards, hazards)) {
            return;
        }
        const auto beforeChange = currentOperationalEventHistoryEntry(selectedEventElementId_);
        current->draft.environment.hazards = hazards;
        if (beforeChange.has_value()) {
            pushOperationalEventUndoEntry(*beforeChange);
        }
        recomputeDiffKeysAfterScenarioChanged(*current);
        restoreOperationalEventSelection(selectedEventElementId_);
        refreshNavigationPanel();
        refreshInspector();
    });
    canvas_->setRouteGuidances(scenario->draft.control.routeGuidances);
    canvas_->setRouteGuidancesChangedHandler([this](const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        if (routeGuidancesEqual(current->draft.control.routeGuidances, guidances)) {
            return;
        }
        const auto beforeChange = currentOperationalEventHistoryEntry(selectedEventElementId_);
        current->draft.control.routeGuidances = guidances;
        if (beforeChange.has_value()) {
            pushOperationalEventUndoEntry(*beforeChange);
        }
        recomputeDiffKeysAfterScenarioChanged(*current);
        restoreOperationalEventSelection(selectedEventElementId_);
        refreshNavigationPanel();
        refreshInspector();
    });
    if (!selectedLayoutElementId_.isEmpty()) {
        canvas_->focusLayoutElement(selectedLayoutElementId_);
    } else if (!selectedCrowdElementId_.isEmpty()) {
        canvas_->focusPlacement(selectedCrowdElementId_);
    }
    shell_->setCanvas(canvas_);
}

void ScenarioAuthoringWidget::refreshInspector() {
    const auto* scenario = currentScenario();
    const bool hasScenario = scenario != nullptr;

    if (scenarioOverviewPanel_ != nullptr) {
        auto* panelLayout = qobject_cast<QVBoxLayout*>(scenarioOverviewPanel_->layout());
        clearLayout(panelLayout);
        if (panelLayout != nullptr) {
            if (!hasScenario) {
                addStatusMessage(panelLayout, "No scenario selected", scenarioOverviewPanel_);
            } else {
                const bool variation = scenarioRoleHasBaselineDiff(scenario->draft.role);
                panelLayout->addWidget(createRoleBadge(
                    scenarioRoleLabel(scenario->draft.role),
                    variation,
                    scenarioOverviewPanel_));

                auto* nameLabel = createLabel(
                    QString::fromStdString(scenario->draft.name),
                    scenarioOverviewPanel_,
                    ui::FontRole::SectionTitle);
                nameLabel->setMinimumWidth(0);
                nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                panelLayout->addWidget(nameLabel);

                addMetaRow(panelLayout, "Population", QString::number(totalOccupantCount(*scenario)), scenarioOverviewPanel_);
                addMetaRow(panelLayout, "Events", QString::number(static_cast<int>(scenario->events.size())), scenarioOverviewPanel_);
                addMetaRow(panelLayout, "Hazards", QString::number(static_cast<int>(scenario->draft.environment.hazards.size())), scenarioOverviewPanel_);
                addMetaRow(panelLayout, "Guidance", QString::number(static_cast<int>(scenario->draft.control.routeGuidances.size())), scenarioOverviewPanel_);
                addMetaRow(panelLayout, "Blocked", QString::number(static_cast<int>(scenario->draft.control.connectionBlocks.size())), scenarioOverviewPanel_);
                addMetaRow(panelLayout, "Start", scenario->startText, scenarioOverviewPanel_);
                addMetaRow(panelLayout, "Destination", scenario->destinationText, scenarioOverviewPanel_);
                if (variation && !scenario->baseScenarioId.isEmpty()) {
                    addMetaRow(panelLayout, "Based on", scenario->baseScenarioId, scenarioOverviewPanel_);
                }
            }
            panelLayout->addStretch(1);
        }
    }

    if (scenarioDiffPanel_ != nullptr) {
        auto* panelLayout = qobject_cast<QVBoxLayout*>(scenarioDiffPanel_->layout());
        clearLayout(panelLayout);
        if (panelLayout != nullptr) {
            auto* title = createLabel("Changes", scenarioDiffPanel_, ui::FontRole::SectionTitle);
            panelLayout->addWidget(title);

            if (!hasScenario) {
                addStatusMessage(panelLayout, "No scenario selected", scenarioDiffPanel_);
            } else if (scenario->draft.role == safecrowd::domain::ScenarioRole::Baseline) {
                addStatusMessage(panelLayout, "Baseline scenario", scenarioDiffPanel_);
            } else if (scenario->baseScenarioId.isEmpty()) {
                addStatusMessage(panelLayout, "Variation scenario / no baseline link", scenarioDiffPanel_);
            } else {
                const auto baseId = scenario->baseScenarioId.toStdString();
                const auto baselineIt = std::find_if(scenarios_.begin(), scenarios_.end(), [&](const auto& candidate) {
                    return candidate.draft.scenarioId == baseId;
                });
                if (scenario->draft.variationDiffKeys.empty()) {
                    addStatusMessage(panelLayout, "No changed fields yet", scenarioDiffPanel_);
                } else {
                    for (const auto& key : scenario->draft.variationDiffKeys) {
                        const auto summary = baselineIt != scenarios_.end()
                            ? buildChangeSummaryLine(baselineIt->draft, scenario->draft, key)
                            : QString::fromStdString(key);
                        addDiffRow(panelLayout, changeCategoryLabel(key), compactChangeSummary(summary), scenarioDiffPanel_);
                    }
                }
            }
            panelLayout->addStretch(1);
        }
    }

    if (elementInspectorPanel_ != nullptr) {
        auto* panelLayout = qobject_cast<QVBoxLayout*>(elementInspectorPanel_->layout());
        clearLayout(panelLayout);
        if (panelLayout != nullptr) {
            if (!hasScenario) {
                addStatusMessage(panelLayout, "No scenario selected", elementInspectorPanel_);
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::None || inspectorSelectionId_.isEmpty()) {
                addStatusMessage(panelLayout, "Select a scenario element on the canvas or in the navigation panel.", elementInspectorPanel_);
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::Layout) {
                auto* title = createLabel("Layout element", elementInspectorPanel_, ui::FontRole::SectionTitle);
                panelLayout->addWidget(title);
                const auto idStd = inspectorSelectionId_.toStdString();
                addMetaRow(panelLayout, "ID", inspectorSelectionId_, elementInspectorPanel_);
                if (inspectorSelectionId_.startsWith("floor:")) {
                    addMetaRow(panelLayout, "Type", "Floor", elementInspectorPanel_);
                    addMetaRow(panelLayout, "Floor", inspectorSelectionId_.mid(QString("floor:").size()), elementInspectorPanel_);
                } else if (const auto zoneIt = std::find_if(layout_.zones.begin(), layout_.zones.end(), [&](const auto& zone) {
                               return zone.id == idStd;
                           });
                           zoneIt != layout_.zones.end()) {
                    addMetaRow(panelLayout, "Type", "Zone", elementInspectorPanel_);
                    addMetaRow(panelLayout, "Name", zoneName(layout_, zoneIt->id), elementInspectorPanel_);
                    addMetaRow(panelLayout, "Floor", QString::fromStdString(zoneIt->floorId), elementInspectorPanel_);
                    addMetaRow(panelLayout, "Capacity", QString::number(static_cast<int>(zoneIt->defaultCapacity)), elementInspectorPanel_);
                } else if (const auto connectionIt = std::find_if(layout_.connections.begin(), layout_.connections.end(), [&](const auto& connection) {
                               return connection.id == idStd;
                           });
                           connectionIt != layout_.connections.end()) {
                    addMetaRow(panelLayout, "Type", "Connection", elementInspectorPanel_);
                    addMetaRow(panelLayout, "Name", connectionLabel(layout_, *connectionIt), elementInspectorPanel_);
                    addMetaRow(panelLayout, "Floor", QString::fromStdString(connectionIt->floorId), elementInspectorPanel_);
                    addMetaRow(panelLayout, "Width", QString("%1 m").arg(connectionIt->effectiveWidth, 0, 'f', 2), elementInspectorPanel_);
                } else if (const auto barrierIt = std::find_if(layout_.barriers.begin(), layout_.barriers.end(), [&](const auto& barrier) {
                               return barrier.id == idStd;
                           });
                           barrierIt != layout_.barriers.end()) {
                    addMetaRow(panelLayout, "Type", "Barrier", elementInspectorPanel_);
                    addMetaRow(panelLayout, "Floor", QString::fromStdString(barrierIt->floorId), elementInspectorPanel_);
                    addMetaRow(panelLayout, "Blocks", barrierIt->blocksMovement ? "Yes" : "No", elementInspectorPanel_);
                } else {
                    addStatusMessage(panelLayout, "Selected layout element was not found.", elementInspectorPanel_);
                }
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::Crowd) {
                auto* scenarioMutable = currentScenario();
                auto placementIt = scenarioMutable == nullptr
                    ? std::vector<ScenarioCrowdPlacement>::iterator{}
                    : std::find_if(scenarioMutable->crowdPlacements.begin(), scenarioMutable->crowdPlacements.end(), [&](const auto& placement) {
                        return placement.id == inspectorSelectionId_;
                    });
                if (scenarioMutable == nullptr || placementIt == scenarioMutable->crowdPlacements.end()) {
                    addStatusMessage(panelLayout, "Selected crowd placement was not found.", elementInspectorPanel_);
                } else {
                    auto* title = createLabel("Occupant", elementInspectorPanel_, ui::FontRole::SectionTitle);
                    panelLayout->addWidget(title);
                    addMetaRow(panelLayout, "ID", placementIt->id, elementInspectorPanel_);
                    addMetaRow(panelLayout, "Type", placementIt->kind == ScenarioCrowdPlacementKind::Source
                        ? "Source"
                        : (placementIt->kind == ScenarioCrowdPlacementKind::Group ? "Group" : "Individual"), elementInspectorPanel_);
                    addMetaRow(panelLayout, "Zone", placementIt->zoneId, elementInspectorPanel_);
                    addMetaRow(panelLayout, "Floor", placementIt->floorId, elementInspectorPanel_);

                    auto* form = new QFormLayout();
                    configureInspectorForm(form);
                    auto* nameEdit = new QLineEdit(elementInspectorPanel_);
                    nameEdit->setText(placementIt->name.isEmpty() ? placementIt->id : placementIt->name);
                    constrainInspectorField(nameEdit);
                    auto* countSpin = new QSpinBox(elementInspectorPanel_);
                    countSpin->setRange(placementIt->kind == ScenarioCrowdPlacementKind::Individual ? 1 : 0, 100000);
                    countSpin->setValue(std::max(0, placementIt->occupantCount));
                    constrainInspectorField(countSpin);
                    form->addRow("Name", nameEdit);
                    form->addRow("Count", countSpin);

                    QDoubleSpinBox* positionXSpin = nullptr;
                    QDoubleSpinBox* positionYSpin = nullptr;
                    if (placementIt->kind != ScenarioCrowdPlacementKind::Group && !placementIt->area.empty()) {
                        positionXSpin = new QDoubleSpinBox(elementInspectorPanel_);
                        positionXSpin->setRange(-100000.0, 100000.0);
                        positionXSpin->setDecimals(2);
                        positionXSpin->setValue(placementIt->area.front().x);
                        constrainInspectorField(positionXSpin);
                        form->addRow("X", positionXSpin);

                        positionYSpin = new QDoubleSpinBox(elementInspectorPanel_);
                        positionYSpin->setRange(-100000.0, 100000.0);
                        positionYSpin->setDecimals(2);
                        positionYSpin->setValue(placementIt->area.front().y);
                        constrainInspectorField(positionYSpin);
                        form->addRow("Y", positionYSpin);
                    }

                    auto* velocityXSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    velocityXSpin->setRange(-20.0, 20.0);
                    velocityXSpin->setDecimals(2);
                    velocityXSpin->setSingleStep(0.1);
                    velocityXSpin->setValue(placementIt->velocity.x);
                    constrainInspectorField(velocityXSpin);
                    form->addRow("Velocity X", velocityXSpin);

                    auto* velocityYSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    velocityYSpin->setRange(-20.0, 20.0);
                    velocityYSpin->setDecimals(2);
                    velocityYSpin->setSingleStep(0.1);
                    velocityYSpin->setValue(placementIt->velocity.y);
                    constrainInspectorField(velocityYSpin);
                    form->addRow("Velocity Y", velocityYSpin);

                    auto* distributionCombo = new QComboBox(elementInspectorPanel_);
                    distributionCombo->addItem("Uniform", static_cast<int>(safecrowd::domain::InitialPlacementDistribution::Uniform));
                    distributionCombo->addItem("Random", static_cast<int>(safecrowd::domain::InitialPlacementDistribution::Random));
                    distributionCombo->setCurrentIndex(std::max(0, distributionCombo->findData(static_cast<int>(placementIt->distribution))));
                    configureInspectorCombo(distributionCombo);
                    QSpinBox* agentsPerSpawnSpin = nullptr;
                    QDoubleSpinBox* startSpin = nullptr;
                    QDoubleSpinBox* endSpin = nullptr;
                    QDoubleSpinBox* intervalSpin = nullptr;
                    if (placementIt->kind != ScenarioCrowdPlacementKind::Source) {
                        form->addRow("Distribution", distributionCombo);
                    } else {
                        agentsPerSpawnSpin = new QSpinBox(elementInspectorPanel_);
                        agentsPerSpawnSpin->setRange(1, 100000);
                        agentsPerSpawnSpin->setValue(std::max(1, placementIt->sourceAgentsPerSpawn));
                        constrainInspectorField(agentsPerSpawnSpin);
                        startSpin = new QDoubleSpinBox(elementInspectorPanel_);
                        startSpin->setRange(0.0, 86400.0);
                        startSpin->setDecimals(1);
                        startSpin->setSuffix(" s");
                        startSpin->setValue(std::max(0.0, placementIt->sourceStartSeconds));
                        constrainInspectorField(startSpin);
                        endSpin = new QDoubleSpinBox(elementInspectorPanel_);
                        endSpin->setRange(0.0, 86400.0);
                        endSpin->setDecimals(1);
                        endSpin->setSuffix(" s");
                        endSpin->setValue(std::max(placementIt->sourceStartSeconds, placementIt->sourceEndSeconds));
                        constrainInspectorField(endSpin);
                        intervalSpin = new QDoubleSpinBox(elementInspectorPanel_);
                        intervalSpin->setRange(0.1, 86400.0);
                        intervalSpin->setDecimals(1);
                        intervalSpin->setSuffix(" s");
                        intervalSpin->setValue(std::max(0.1, placementIt->sourceIntervalSeconds));
                        constrainInspectorField(intervalSpin);
                        form->addRow("Per spawn", agentsPerSpawnSpin);
                        form->addRow("Start", startSpin);
                        form->addRow("End", endSpin);
                        form->addRow("Interval", intervalSpin);
                    }
                    panelLayout->addLayout(form);

                    auto* applyButton = new QPushButton("Apply Changes", elementInspectorPanel_);
                    applyButton->setFont(ui::font(ui::FontRole::Body));
                    applyButton->setStyleSheet(ui::secondaryButtonStyleSheet());
                    configureInspectorActionButton(applyButton);
                    panelLayout->addWidget(applyButton);
                    const auto placementId = inspectorSelectionId_;
                    connect(applyButton, &QPushButton::clicked, this, [this, placementId, nameEdit, countSpin, positionXSpin, positionYSpin, velocityXSpin, velocityYSpin, distributionCombo, agentsPerSpawnSpin, startSpin, endSpin, intervalSpin]() {
                        auto* scenario = currentScenario();
                        if (scenario == nullptr) {
                            return;
                        }
                        auto placementIt = std::find_if(scenario->crowdPlacements.begin(), scenario->crowdPlacements.end(), [&](const auto& placement) {
                            return placement.id == placementId;
                        });
                        if (placementIt == scenario->crowdPlacements.end()) {
                            return;
                        }
                        const auto beforeChange = currentCrowdPlacementHistoryEntry(placementId);
                        const auto previousPlacement = *placementIt;
                        placementIt->name = nameEdit->text().trimmed();
                        placementIt->occupantCount = countSpin->value();
                        if (positionXSpin != nullptr && positionYSpin != nullptr
                            && placementIt->kind != ScenarioCrowdPlacementKind::Group) {
                            const safecrowd::domain::Point2D position{
                                .x = positionXSpin->value(),
                                .y = positionYSpin->value(),
                            };
                            const auto* zone = findZoneContainingPoint(layout_, position);
                            if (zone == nullptr) {
                                QMessageBox::warning(this, "Edit crowd", "The placement location must stay inside a walkable zone.");
                                return;
                            }
                            placementIt->area = {position};
                            placementIt->zoneId = QString::fromStdString(zone->id);
                            placementIt->floorId = QString::fromStdString(zone->floorId);
                        }
                        placementIt->velocity = {
                            .x = velocityXSpin->value(),
                            .y = velocityYSpin->value(),
                        };
                        if (placementIt->kind != ScenarioCrowdPlacementKind::Source) {
                            placementIt->distribution = static_cast<safecrowd::domain::InitialPlacementDistribution>(
                                distributionCombo->currentData().toInt());
                        } else {
                            if (agentsPerSpawnSpin != nullptr) {
                                placementIt->sourceAgentsPerSpawn = agentsPerSpawnSpin->value();
                            }
                            if (startSpin != nullptr) {
                                placementIt->sourceStartSeconds = std::max(0.0, startSpin->value());
                            }
                            if (endSpin != nullptr) {
                                placementIt->sourceEndSeconds = std::max(placementIt->sourceStartSeconds, endSpin->value());
                            }
                            if (intervalSpin != nullptr) {
                                placementIt->sourceIntervalSeconds = std::max(0.1, intervalSpin->value());
                            }
                        }
                        if (crowdPlacementEqual(*placementIt, previousPlacement)) {
                            return;
                        }
                        selectedCrowdElementId_ = placementId;
                        inspectorSelectionKind_ = InspectorSelectionKind::Crowd;
                        inspectorSelectionId_ = placementId;
                        updateCurrentScenarioPlacements(scenario->crowdPlacements, beforeChange, placementId);
                        if (canvas_ != nullptr) {
                            canvas_->setPlacements(scenario->crowdPlacements);
                            canvas_->focusPlacement(placementId);
                        }
                    });
                }
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::ConnectionBlock) {
                const auto idStd = inspectorSelectionId_.toStdString();
                const auto blockIt = std::find_if(scenario->draft.control.connectionBlocks.begin(), scenario->draft.control.connectionBlocks.end(), [&](const auto& block) {
                    return block.id == idStd;
                });
                if (blockIt == scenario->draft.control.connectionBlocks.end()) {
                    addStatusMessage(panelLayout, "Selected block was not found.", elementInspectorPanel_);
                } else {
                    panelLayout->addWidget(createLabel("Blocked Door / Exit", elementInspectorPanel_, ui::FontRole::SectionTitle));
                    addMetaRow(panelLayout, "ID", inspectorSelectionId_, elementInspectorPanel_);

                    auto* form = new QFormLayout();
                    configureInspectorForm(form);

                    auto* targetCombo = createConnectionCombo(elementInspectorPanel_, layout_, blockIt->connectionId);
                    form->addRow("Target", targetCombo);

                    auto* alwaysCheck = new QCheckBox("Always blocked", elementInspectorPanel_);
                    alwaysCheck->setChecked(blockIt->intervals.empty());
                    form->addRow("", alwaysCheck);

                    auto* intervalEdit = new QPlainTextEdit(elementInspectorPanel_);
                    intervalEdit->setPlainText(intervalsToEditorText(blockIt->intervals));
                    intervalEdit->setPlaceholderText("0 - 30\n60 - 90");
                    intervalEdit->setMinimumHeight(74);
                    intervalEdit->setEnabled(!alwaysCheck->isChecked());
                    configureInspectorTextEdit(intervalEdit);
                    form->addRow("Intervals", intervalEdit);
                    connect(alwaysCheck, &QCheckBox::toggled, intervalEdit, [intervalEdit](bool checked) {
                        intervalEdit->setEnabled(!checked);
                    });

                    auto* help = createLabel("One interval per line, in seconds. Leave Always blocked on for a permanent closure.", elementInspectorPanel_, ui::FontRole::Caption);
                    help->setStyleSheet(ui::mutedTextStyleSheet());
                    form->addRow("", help);
                    panelLayout->addLayout(form);

                    auto* applyButton = new QPushButton("Apply Changes", elementInspectorPanel_);
                    applyButton->setFont(ui::font(ui::FontRole::Body));
                    applyButton->setStyleSheet(ui::secondaryButtonStyleSheet());
                    configureInspectorActionButton(applyButton);
                    panelLayout->addWidget(applyButton);
                    const auto blockId = inspectorSelectionId_;
                    connect(applyButton, &QPushButton::clicked, this, [this, blockId, targetCombo, alwaysCheck, intervalEdit]() {
                        auto* scenario = currentScenario();
                        if (scenario == nullptr) {
                            return;
                        }
                        auto& blocks = scenario->draft.control.connectionBlocks;
                        auto blockIt = std::find_if(blocks.begin(), blocks.end(), [&](const auto& block) {
                            return QString::fromStdString(block.id) == blockId;
                        });
                        if (blockIt == blocks.end()) {
                            return;
                        }

                        const auto targetConnectionId = targetCombo->currentData().toString();
                        if (findConnection(layout_, targetConnectionId) == nullptr) {
                            QMessageBox::warning(this, "Edit blocked door", "Select a valid door or exit.");
                            return;
                        }

                        std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals;
                        if (!alwaysCheck->isChecked()) {
                            QString errorMessage;
                            if (!parseIntervalEditorText(intervalEdit->toPlainText(), &intervals, &errorMessage)) {
                                QMessageBox::warning(this, "Edit blocked door", errorMessage);
                                return;
                            }
                            if (intervals.empty()) {
                                QMessageBox::warning(this, "Edit blocked door", "Add at least one interval, or turn on Always blocked.");
                                return;
                            }
                        }

                        const auto beforeChange = currentOperationalEventHistoryEntry(blockId);
                        const auto previousBlock = *blockIt;
                        blockIt->connectionId = targetConnectionId.toStdString();
                        blockIt->intervals = std::move(intervals);
                        if (connectionBlockEqual(*blockIt, previousBlock)) {
                            return;
                        }
                        if (beforeChange.has_value()) {
                            pushOperationalEventUndoEntry(*beforeChange);
                        }
                        selectedEventElementId_ = blockId;
                        inspectorSelectionKind_ = InspectorSelectionKind::ConnectionBlock;
                        inspectorSelectionId_ = blockId;
                        if (canvas_ != nullptr) {
                            canvas_->setConnectionBlocks(blocks);
                        }
                        recomputeDiffKeysAfterScenarioChanged(*scenario);
                        refreshNavigationPanel();
                        refreshInspector();
                    });
                }
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::EnvironmentHazard) {
                const auto idStd = inspectorSelectionId_.toStdString();
                auto* scenarioMutable = currentScenario();
                auto hazardIt = scenarioMutable == nullptr
                    ? std::vector<safecrowd::domain::EnvironmentHazardDraft>::iterator{}
                    : std::find_if(scenarioMutable->draft.environment.hazards.begin(), scenarioMutable->draft.environment.hazards.end(), [&](const auto& hazard) {
                        return hazard.id == idStd;
                    });
                if (scenarioMutable == nullptr || hazardIt == scenarioMutable->draft.environment.hazards.end()) {
                    addStatusMessage(panelLayout, "Selected hazard was not found.", elementInspectorPanel_);
                } else {
                    panelLayout->addWidget(createLabel("Hazard", elementInspectorPanel_, ui::FontRole::SectionTitle));
                    addMetaRow(panelLayout, "ID", inspectorSelectionId_, elementInspectorPanel_);

                    auto* form = new QFormLayout();
                    configureInspectorForm(form);

                    auto* nameEdit = new QLineEdit(elementInspectorPanel_);
                    nameEdit->setText(QString::fromStdString(hazardIt->name));
                    constrainInspectorField(nameEdit);
                    form->addRow("Name", nameEdit);

                    auto* zoneCombo = createZoneCombo(elementInspectorPanel_, layout_, hazardIt->affectedZoneId);
                    form->addRow("Zone", zoneCombo);

                    auto* xSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    xSpin->setRange(-100000.0, 100000.0);
                    xSpin->setDecimals(2);
                    xSpin->setValue(hazardIt->position.x);
                    constrainInspectorField(xSpin);
                    form->addRow("X", xSpin);

                    auto* ySpin = new QDoubleSpinBox(elementInspectorPanel_);
                    ySpin->setRange(-100000.0, 100000.0);
                    ySpin->setDecimals(2);
                    ySpin->setValue(hazardIt->position.y);
                    constrainInspectorField(ySpin);
                    form->addRow("Y", ySpin);

                    auto* startSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    startSpin->setRange(0.0, 86400.0);
                    startSpin->setDecimals(1);
                    startSpin->setSuffix(" s");
                    startSpin->setValue(std::max(0.0, hazardIt->startSeconds));
                    constrainInspectorField(startSpin);
                    form->addRow("Start", startSpin);

                    const auto openEnded = safecrowd::domain::environmentHazardHasOpenEndedSchedule(*hazardIt);
                    auto* endSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    endSpin->setRange(0.0, 86400.0);
                    endSpin->setDecimals(1);
                    endSpin->setSuffix(" s");
                    endSpin->setValue(openEnded ? std::max(0.0, hazardIt->startSeconds) : std::max(hazardIt->startSeconds, hazardIt->endSeconds));
                    endSpin->setEnabled(!openEnded);
                    constrainInspectorField(endSpin);
                    form->addRow("End", endSpin);

                    auto* openEndedCheck = new QCheckBox("Open ended", elementInspectorPanel_);
                    openEndedCheck->setChecked(openEnded);
                    form->addRow("", openEndedCheck);
                    connect(openEndedCheck, &QCheckBox::toggled, endSpin, [startSpin, endSpin](bool checked) {
                        endSpin->setEnabled(!checked);
                        if (checked) {
                            endSpin->setValue(startSpin->value());
                        }
                    });
                    connect(startSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), endSpin, [openEndedCheck, endSpin](double value) {
                        if (openEndedCheck->isChecked()) {
                            endSpin->setValue(value);
                        }
                    });

                    QSlider* severitySlider = nullptr;
                    form->addRow("Severity", createSliderEditor(
                        elementInspectorPanel_,
                        &severitySlider,
                        nullptr,
                        0,
                        2,
                        severitySliderValue(hazardIt->severity),
                        [](int value) {
                            return severityLabel(severityFromSliderValue(value));
                        }));

                    auto* radiusSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    radiusSpin->setRange(0.0, 10000.0);
                    radiusSpin->setDecimals(1);
                    radiusSpin->setSingleStep(0.5);
                    radiusSpin->setSuffix(" m");
                    radiusSpin->setValue(safecrowd::domain::environmentHazardRadiusMeters(*hazardIt));
                    constrainInspectorField(radiusSpin);
                    form->addRow("Radius", radiusSpin);

                    auto* noteEdit = new QPlainTextEdit(elementInspectorPanel_);
                    noteEdit->setPlainText(QString::fromStdString(hazardIt->note));
                    noteEdit->setMinimumHeight(74);
                    configureInspectorTextEdit(noteEdit);
                    form->addRow("Note", noteEdit);
                    panelLayout->addLayout(form);

                    auto* applyButton = new QPushButton("Apply Changes", elementInspectorPanel_);
                    applyButton->setFont(ui::font(ui::FontRole::Body));
                    applyButton->setStyleSheet(ui::secondaryButtonStyleSheet());
                    configureInspectorActionButton(applyButton);
                    panelLayout->addWidget(applyButton);
                    const auto hazardId = inspectorSelectionId_;
                    connect(applyButton, &QPushButton::clicked, this, [this, hazardId, nameEdit, zoneCombo, xSpin, ySpin, startSpin, endSpin, openEndedCheck, severitySlider, radiusSpin, noteEdit]() {
                        auto* scenario = currentScenario();
                        if (scenario == nullptr) {
                            return;
                        }
                        auto& hazards = scenario->draft.environment.hazards;
                        auto hazardIt = std::find_if(hazards.begin(), hazards.end(), [&](auto& hazard) {
                            return QString::fromStdString(hazard.id) == hazardId;
                        });
                        if (hazardIt == hazards.end()) {
                            return;
                        }

                        const auto name = nameEdit->text().trimmed();
                        if (name.isEmpty()) {
                            QMessageBox::warning(this, "Edit hazard", "Enter a hazard name.");
                            return;
                        }
                        const auto zoneId = zoneCombo->currentData().toString();
                        const auto* zone = findZone(layout_, zoneId);
                        if (zone == nullptr) {
                            QMessageBox::warning(this, "Edit hazard", "Select a valid affected zone.");
                            return;
                        }

                        const safecrowd::domain::Point2D position{
                            .x = xSpin->value(),
                            .y = ySpin->value(),
                        };
                        if (!pointInPolygon(zone->area, position)) {
                            QMessageBox::warning(this, "Edit hazard", "The hazard location must stay inside the affected zone.");
                            return;
                        }
                        if (!openEndedCheck->isChecked() && endSpin->value() <= startSpin->value()) {
                            QMessageBox::warning(this, "Edit hazard", "Set the end time after the start time.");
                            return;
                        }

                        const auto beforeChange = currentOperationalEventHistoryEntry(hazardId);
                        const auto previousHazard = *hazardIt;
                        hazardIt->name = name.toStdString();
                        hazardIt->affectedZoneId = zoneId.toStdString();
                        hazardIt->floorId = zone->floorId;
                        hazardIt->position = position;
                        hazardIt->startSeconds = std::max(0.0, startSpin->value());
                        hazardIt->endSeconds = openEndedCheck->isChecked()
                            ? hazardIt->startSeconds
                            : std::max(hazardIt->startSeconds, endSpin->value());
                        hazardIt->severity = severityFromSliderValue(severitySlider != nullptr ? severitySlider->value() : 1);
                        hazardIt->radiusMeters = std::max(0.0, radiusSpin->value());
                        hazardIt->note = noteEdit->toPlainText().trimmed().toStdString();
                        if (environmentHazardEqual(*hazardIt, previousHazard)) {
                            return;
                        }
                        if (beforeChange.has_value()) {
                            pushOperationalEventUndoEntry(*beforeChange);
                        }
                        if (canvas_ != nullptr) {
                            canvas_->setEnvironmentHazards(hazards);
                        }
                        selectedEventElementId_ = hazardId;
                        inspectorSelectionKind_ = InspectorSelectionKind::EnvironmentHazard;
                        inspectorSelectionId_ = hazardId;
                        recomputeDiffKeysAfterScenarioChanged(*scenario);
                        refreshNavigationPanel();
                        refreshInspector();
                    });
                }
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::RouteGuidance) {
                const auto idStd = inspectorSelectionId_.toStdString();
                const auto guidanceIt = std::find_if(scenario->draft.control.routeGuidances.begin(), scenario->draft.control.routeGuidances.end(), [&](const auto& guidance) {
                    return guidance.id == idStd;
                });
                if (guidanceIt == scenario->draft.control.routeGuidances.end()) {
                    addStatusMessage(panelLayout, "Selected route guidance was not found.", elementInspectorPanel_);
                } else {
                    panelLayout->addWidget(createLabel("Route Guidance", elementInspectorPanel_, ui::FontRole::SectionTitle));
                    addMetaRow(panelLayout, "ID", inspectorSelectionId_, elementInspectorPanel_);

                    auto* form = new QFormLayout();
                    configureInspectorForm(form);

                    auto* exitCombo = createExitZoneCombo(elementInspectorPanel_, layout_, guidanceIt->guidedExitZoneId);
                    form->addRow("Exit", exitCombo);

                    auto* connectionCombo = createConnectionCombo(
                        elementInspectorPanel_,
                        layout_,
                        guidanceIt->installConnectionId,
                        true,
                        "Use zone position");
                    form->addRow("Install", connectionCombo);

                    auto* zoneCombo = createZoneCombo(elementInspectorPanel_, layout_, guidanceIt->installZoneId);
                    form->addRow("Zone", zoneCombo);

                    auto* xSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    xSpin->setRange(-100000.0, 100000.0);
                    xSpin->setDecimals(2);
                    xSpin->setValue(guidanceIt->installPosition.x);
                    constrainInspectorField(xSpin);
                    form->addRow("X", xSpin);

                    auto* ySpin = new QDoubleSpinBox(elementInspectorPanel_);
                    ySpin->setRange(-100000.0, 100000.0);
                    ySpin->setDecimals(2);
                    ySpin->setValue(guidanceIt->installPosition.y);
                    constrainInspectorField(ySpin);
                    form->addRow("Y", ySpin);

                    const bool installedOnConnection = !connectionCombo->currentData().toString().isEmpty();
                    if (installedOnConnection) {
                        if (const auto* connection = findConnection(layout_, connectionCombo->currentData().toString()); connection != nullptr) {
                            const auto center = connectionCenter(*connection);
                            xSpin->setValue(center.x);
                            ySpin->setValue(center.y);
                        }
                    }
                    zoneCombo->setEnabled(!installedOnConnection);
                    xSpin->setEnabled(!installedOnConnection);
                    ySpin->setEnabled(!installedOnConnection);
                    connect(connectionCombo, qOverload<int>(&QComboBox::currentIndexChanged), xSpin, [this, connectionCombo, zoneCombo, xSpin, ySpin](int) {
                        const auto connectionId = connectionCombo->currentData().toString();
                        const bool useConnection = !connectionId.isEmpty();
                        zoneCombo->setEnabled(!useConnection);
                        xSpin->setEnabled(!useConnection);
                        ySpin->setEnabled(!useConnection);
                        if (const auto* connection = findConnection(layout_, connectionId); connection != nullptr) {
                            const auto center = connectionCenter(*connection);
                            xSpin->setValue(center.x);
                            ySpin->setValue(center.y);
                        }
                    });

                    QSlider* complianceSlider = nullptr;
                    form->addRow("Compliance", createSliderEditor(
                        elementInspectorPanel_,
                        &complianceSlider,
                        nullptr,
                        0,
                        100,
                        static_cast<int>(std::lround(std::clamp(guidanceIt->baseComplianceRate, 0.0, 1.0) * 100.0)),
                        [](int value) {
                            return QString("%1%").arg(value);
                        }));

                    auto* radiusSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    radiusSpin->setRange(0.0, 10000.0);
                    radiusSpin->setDecimals(1);
                    radiusSpin->setSingleStep(0.5);
                    radiusSpin->setSuffix(" m");
                    radiusSpin->setValue(std::max(0.0, guidanceIt->influenceRadiusMeters));
                    constrainInspectorField(radiusSpin);
                    form->addRow("Radius", radiusSpin);

                    auto* maxDetourSpin = new QDoubleSpinBox(elementInspectorPanel_);
                    maxDetourSpin->setRange(0.0, 10000.0);
                    maxDetourSpin->setDecimals(1);
                    maxDetourSpin->setSingleStep(1.0);
                    maxDetourSpin->setSuffix(" m");
                    maxDetourSpin->setValue(std::max(0.0, guidanceIt->maxDetourMeters));
                    constrainInspectorField(maxDetourSpin);
                    form->addRow("Max detour", maxDetourSpin);

                    std::vector<safecrowd::domain::RouteGuidancePeriodDraft> periodsForUi = guidanceIt->periods;
                    if (periodsForUi.empty() && (guidanceIt->startSeconds > 0.0 || guidanceIt->endSeconds > 0.0)) {
                        periodsForUi.push_back({
                            .startSeconds = std::max(0.0, guidanceIt->startSeconds),
                            .endSeconds = std::max(std::max(0.0, guidanceIt->startSeconds), guidanceIt->endSeconds),
                        });
                    }

                    auto* alwaysActiveCheck = new QCheckBox("Always active", elementInspectorPanel_);
                    alwaysActiveCheck->setChecked(periodsForUi.empty());
                    form->addRow("", alwaysActiveCheck);

                    auto* periodEdit = new QPlainTextEdit(elementInspectorPanel_);
                    periodEdit->setPlainText(routeGuidancePeriodsToEditorText(periodsForUi));
                    periodEdit->setPlaceholderText("0 - 120\n240 - 360");
                    periodEdit->setMinimumHeight(74);
                    periodEdit->setEnabled(!alwaysActiveCheck->isChecked());
                    configureInspectorTextEdit(periodEdit);
                    form->addRow("Periods", periodEdit);
                    connect(alwaysActiveCheck, &QCheckBox::toggled, periodEdit, [periodEdit](bool checked) {
                        periodEdit->setEnabled(!checked);
                    });

                    auto* help = createLabel("Periods use one start-end pair per line, in seconds.", elementInspectorPanel_, ui::FontRole::Caption);
                    help->setStyleSheet(ui::mutedTextStyleSheet());
                    form->addRow("", help);
                    panelLayout->addLayout(form);

                    auto* applyButton = new QPushButton("Apply Changes", elementInspectorPanel_);
                    applyButton->setFont(ui::font(ui::FontRole::Body));
                    applyButton->setStyleSheet(ui::secondaryButtonStyleSheet());
                    configureInspectorActionButton(applyButton);
                    panelLayout->addWidget(applyButton);
                    const auto guidanceId = inspectorSelectionId_;
                    connect(applyButton, &QPushButton::clicked, this, [this, guidanceId, exitCombo, connectionCombo, zoneCombo, xSpin, ySpin, complianceSlider, radiusSpin, maxDetourSpin, alwaysActiveCheck, periodEdit]() {
                        auto* scenario = currentScenario();
                        if (scenario == nullptr) {
                            return;
                        }
                        auto& guidances = scenario->draft.control.routeGuidances;
                        auto guidanceIt = std::find_if(guidances.begin(), guidances.end(), [&](auto& guidance) {
                            return QString::fromStdString(guidance.id) == guidanceId;
                        });
                        if (guidanceIt == guidances.end()) {
                            return;
                        }

                        const auto exitZoneId = exitCombo->currentData().toString();
                        if (!exitZoneId.isEmpty()) {
                            const auto* exitZone = findZone(layout_, exitZoneId);
                            if (exitZone == nullptr || exitZone->kind != safecrowd::domain::ZoneKind::Exit) {
                                QMessageBox::warning(this, "Edit route guidance", "Select a valid target exit.");
                                return;
                            }
                        }

                        const auto connectionId = connectionCombo->currentData().toString();
                        if (!connectionId.isEmpty()) {
                            const auto* connection = findConnection(layout_, connectionId);
                            if (connection == nullptr) {
                                QMessageBox::warning(this, "Edit route guidance", "Select a valid install connection.");
                                return;
                            }
                            guidanceIt->installConnectionId = connectionId.toStdString();
                            guidanceIt->installZoneId.clear();
                            guidanceIt->installFloorId = connection->floorId;
                            guidanceIt->installPosition = connectionCenter(*connection);
                        } else {
                            const auto zoneId = zoneCombo->currentData().toString();
                            const auto* zone = findZone(layout_, zoneId);
                            if (zone == nullptr) {
                                QMessageBox::warning(this, "Edit route guidance", "Select a valid install zone.");
                                return;
                            }
                            const safecrowd::domain::Point2D position{
                                .x = xSpin->value(),
                                .y = ySpin->value(),
                            };
                            if (!pointInPolygon(zone->area, position)) {
                                QMessageBox::warning(this, "Edit route guidance", "The guidance location must stay inside the install zone.");
                                return;
                            }
                            guidanceIt->installConnectionId.clear();
                            guidanceIt->installZoneId = zoneId.toStdString();
                            guidanceIt->installFloorId = zone->floorId;
                            guidanceIt->installPosition = position;
                        }

                        std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals;
                        if (!alwaysActiveCheck->isChecked()) {
                            QString errorMessage;
                            if (!parseIntervalEditorText(periodEdit->toPlainText(), &intervals, &errorMessage)) {
                                QMessageBox::warning(this, "Edit route guidance", errorMessage);
                                return;
                            }
                            if (intervals.empty()) {
                                QMessageBox::warning(this, "Edit route guidance", "Add at least one active period, or turn on Always active.");
                                return;
                            }
                        }

                        const auto beforeChange = currentOperationalEventHistoryEntry(guidanceId);
                        const auto previousGuidance = *guidanceIt;
                        guidanceIt->guidedExitZoneId = exitZoneId.toStdString();
                        guidanceIt->baseComplianceRate = std::clamp(
                            static_cast<double>(complianceSlider != nullptr ? complianceSlider->value() : 50) / 100.0,
                            0.0,
                            1.0);
                        guidanceIt->influenceRadiusMeters = std::max(0.0, radiusSpin->value());
                        guidanceIt->maxDetourMeters = std::max(0.0, maxDetourSpin->value());
                        guidanceIt->periods = routeGuidancePeriodsFromIntervals(intervals);
                        if (guidanceIt->periods.empty()) {
                            guidanceIt->startSeconds = 0.0;
                            guidanceIt->endSeconds = 0.0;
                        } else {
                            guidanceIt->startSeconds = guidanceIt->periods.front().startSeconds;
                            guidanceIt->endSeconds = guidanceIt->periods.front().endSeconds;
                        }
                        if (routeGuidanceEqual(*guidanceIt, previousGuidance)) {
                            return;
                        }
                        if (beforeChange.has_value()) {
                            pushOperationalEventUndoEntry(*beforeChange);
                        }

                        selectedEventElementId_ = guidanceId;
                        inspectorSelectionKind_ = InspectorSelectionKind::RouteGuidance;
                        inspectorSelectionId_ = guidanceId;
                        if (canvas_ != nullptr) {
                            canvas_->setRouteGuidances(guidances);
                        }
                        recomputeDiffKeysAfterScenarioChanged(*scenario);
                        refreshNavigationPanel();
                        refreshInspector();
                    });
                }
            } else if (inspectorSelectionKind_ == InspectorSelectionKind::OperationalEvent) {
                auto* scenarioMutable = currentScenario();
                const auto idStd = inspectorSelectionId_.toStdString();
                auto eventIt = scenarioMutable == nullptr
                    ? std::vector<safecrowd::domain::OperationalEventDraft>::iterator{}
                    : std::find_if(scenarioMutable->events.begin(), scenarioMutable->events.end(), [&](const auto& event) {
                        return event.id == idStd;
                    });
                if (scenarioMutable == nullptr || eventIt == scenarioMutable->events.end()) {
                    addStatusMessage(panelLayout, "Selected event was not found.", elementInspectorPanel_);
                } else {
                    panelLayout->addWidget(createLabel("Operational Event", elementInspectorPanel_, ui::FontRole::SectionTitle));
                    addMetaRow(panelLayout, "ID", inspectorSelectionId_, elementInspectorPanel_);

                    auto* form = new QFormLayout();
                    configureInspectorForm(form);

                    auto* nameEdit = new QLineEdit(elementInspectorPanel_);
                    nameEdit->setText(QString::fromStdString(eventIt->name));
                    constrainInspectorField(nameEdit);
                    form->addRow("Name", nameEdit);

                    auto* triggerEdit = new QPlainTextEdit(elementInspectorPanel_);
                    triggerEdit->setPlainText(QString::fromStdString(eventIt->triggerSummary));
                    triggerEdit->setMinimumHeight(70);
                    configureInspectorTextEdit(triggerEdit);
                    form->addRow("Trigger", triggerEdit);

                    auto* targetEdit = new QPlainTextEdit(elementInspectorPanel_);
                    targetEdit->setPlainText(QString::fromStdString(eventIt->targetSummary));
                    targetEdit->setMinimumHeight(70);
                    configureInspectorTextEdit(targetEdit);
                    form->addRow("Target", targetEdit);
                    panelLayout->addLayout(form);

                    auto* applyButton = new QPushButton("Apply Changes", elementInspectorPanel_);
                    applyButton->setFont(ui::font(ui::FontRole::Body));
                    applyButton->setStyleSheet(ui::secondaryButtonStyleSheet());
                    configureInspectorActionButton(applyButton);
                    panelLayout->addWidget(applyButton);
                    const auto eventId = inspectorSelectionId_;
                    connect(applyButton, &QPushButton::clicked, this, [this, eventId, nameEdit, triggerEdit, targetEdit]() {
                        auto* scenario = currentScenario();
                        if (scenario == nullptr) {
                            return;
                        }
                        auto eventIt = std::find_if(scenario->events.begin(), scenario->events.end(), [&](auto& event) {
                            return QString::fromStdString(event.id) == eventId;
                        });
                        if (eventIt == scenario->events.end()) {
                            return;
                        }
                        const auto name = nameEdit->text().trimmed();
                        if (name.isEmpty()) {
                            QMessageBox::warning(this, "Edit event", "Enter an event name.");
                            return;
                        }
                        const auto beforeChange = currentOperationalEventHistoryEntry(eventId);
                        const auto previousName = eventIt->name;
                        const auto previousTrigger = eventIt->triggerSummary;
                        const auto previousTarget = eventIt->targetSummary;
                        eventIt->name = name.toStdString();
                        eventIt->triggerSummary = triggerEdit->toPlainText().trimmed().toStdString();
                        eventIt->targetSummary = targetEdit->toPlainText().trimmed().toStdString();
                        if (eventIt->name == previousName
                            && eventIt->triggerSummary == previousTrigger
                            && eventIt->targetSummary == previousTarget) {
                            return;
                        }
                        if (beforeChange.has_value()) {
                            pushOperationalEventUndoEntry(*beforeChange);
                        }
                        synchronizeOperationalEvents(*scenario);
                        restoreOperationalEventSelection(eventId);
                        refreshNavigationPanel();
                        refreshInspector();
                    });
                }
            }
            panelLayout->addStretch(1);
        }
    }

    if (newScenarioButton_ != nullptr) {
        newScenarioButton_->setText(hasScenario ? "New Scenario from Current" : "New Scenario");
    }
    if (stageScenarioButton_ != nullptr) {
        const bool staged = hasScenario && scenario->stagedForRun;
        const bool hasOccupants = hasScenario && scenarioHasOccupants(*scenario);
        stageScenarioButton_->setEnabled(hasScenario && (staged || hasOccupants));
        stageScenarioButton_->setText(staged ? "Unstage" : "Stage Scenario");
        stageScenarioButton_->setToolTip(hasScenario && !staged && !hasOccupants
            ? "Add at least one occupant before staging this scenario."
            : QString{});
        stageScenarioButton_->setStyleSheet(staged
            ? QString(
                "QPushButton { background: #b42318; border: 1px solid #b42318; border-radius: 12px; color: white; font-weight: 600; padding: 10px 18px; }"
                "QPushButton:hover { background: #9f1f16; }"
                "QPushButton:disabled { background: #d7e0ea; border-color: #d7e0ea; color: #8a98a8; }")
            : QString(
                "QPushButton { background: #15803d; border: 1px solid #15803d; border-radius: 12px; color: white; font-weight: 600; padding: 10px 18px; }"
                "QPushButton:hover { background: #166534; }"
                "QPushButton:disabled { background: #d7e0ea; border-color: #d7e0ea; color: #8a98a8; }"));
    }
    const auto stagedCount = std::count_if(scenarios_.begin(), scenarios_.end(), [](const auto& scenario) {
        return scenario.stagedForRun && scenarioHasOccupants(scenario);
    });
    if (stagedScenariosLabel_ != nullptr) {
        QStringList lines;
        if (stagedCount == 0) {
            lines << "No staged scenarios";
        } else {
            lines << "Staged scenarios";
            for (const auto& stagedScenario : scenarios_) {
                if (!stagedScenario.stagedForRun || !scenarioHasOccupants(stagedScenario)) {
                    continue;
                }
                lines << QString("- %1 (%2)")
                    .arg(QString::fromStdString(stagedScenario.draft.name), scenarioRoleLabel(stagedScenario.draft.role));
            }
        }
        stagedScenariosLabel_->setText(lines.join('\n'));
    }
    if (executeRunButton_ != nullptr) {
        executeRunButton_->setEnabled(stagedCount > 0);
    }
}

void ScenarioAuthoringWidget::refreshNavigationPanel() {
    if (shell_ == nullptr) {
        return;
    }

    const auto activeTabId = [this]() {
        switch (navigationView_) {
        case NavigationView::Crowd:
            return QString("crowd");
        case NavigationView::Events:
            return QString("events");
        case NavigationView::Layout:
        default:
            return QString("layout");
        }
    }();
    shell_->setNavigationTabs(
        {
            {
                .id = "layout",
                .label = "Layout",
                .icon = QIcon{},
            },
            {
                .id = "crowd",
                .label = "Occupant",
                .icon = scenarioNavigationIcon(QStringLiteral(":/tool-icons/scenario-authoring/crowd.svg"), QColor("#1f5fae")),
            },
            {
                .id = "events",
                .label = "Events / Hazards",
                .icon = scenarioNavigationIcon(QStringLiteral(":/tool-icons/scenario-authoring/events.svg"), QColor("#1f5fae")),
            },
        },
        activeTabId,
        [this](const QString& tabId) {
            if (tabId == "crowd") {
                navigationView_ = NavigationView::Crowd;
            } else if (tabId == "events") {
                navigationView_ = NavigationView::Events;
            } else {
                navigationView_ = NavigationView::Layout;
            }
            refreshNavigationPanel();
        });

    if (navigationView_ == NavigationView::Layout) {
        shell_->setNavigationPanel(new LayoutNavigationPanelWidget(
            &layout_,
            [this](const QString& elementId) {
                selectedLayoutElementId_ = elementId;
                selectedCrowdElementId_.clear();
                selectedEventElementId_.clear();
                inspectorSelectionKind_ = elementId.isEmpty()
                    ? InspectorSelectionKind::None
                    : InspectorSelectionKind::Layout;
                inspectorSelectionId_ = elementId;
                if (canvas_ != nullptr) {
                    canvas_->activateLayoutElement(elementId);
                }
                refreshInspector();
            },
            shell_,
            shell_->createPanelHeader("Layout", shell_, false),
            NavigationTreeState{
                .expandedNodeIds = layoutExpandedNodeIds_,
                .selectedId = selectedLayoutElementId_,
                .restoreExpandedState = true,
            },
            [this](const QSet<QString>& expandedNodeIds) {
                layoutExpandedNodeIds_ = expandedNodeIds;
            }));
        return;
    }
    if (navigationView_ == NavigationView::Crowd) {
        shell_->setNavigationPanel(createCrowdPanel(
            currentScenario(),
            [this](const QString& placementId) {
                selectedCrowdElementId_ = placementId;
                selectedLayoutElementId_.clear();
                selectedEventElementId_.clear();
                inspectorSelectionKind_ = placementId.isEmpty()
                    ? InspectorSelectionKind::None
                    : InspectorSelectionKind::Crowd;
                inspectorSelectionId_ = placementId.section('/', 0, 0);
                if (canvas_ != nullptr) {
                    canvas_->focusPlacement(placementId);
                }
                refreshInspector();
            },
            NavigationTreeState{
                .expandedNodeIds = crowdExpandedNodeIds_,
                .selectedId = selectedCrowdElementId_,
                .restoreExpandedState = true,
            },
            [this](const QSet<QString>& expandedNodeIds) {
                crowdExpandedNodeIds_ = expandedNodeIds;
            },
            [this](const QString& crowdElementId) {
                if (canvas_ != nullptr) {
                    canvas_->deleteCrowdElementById(crowdElementId);
                }
            },
            shell_,
            shell_));
        return;
    }
    shell_->setNavigationPanel(createEventsPanel(
        layout_,
        currentScenario(),
        shell_,
        shell_,
        [this](const QString& rawId) {
            setInspectorSelectionFromEventId(rawId);
        },
        NavigationTreeState{
            .expandedNodeIds = eventExpandedNodeIds_,
            .selectedId = selectedEventElementId_,
            .restoreExpandedState = true,
        },
        [this](const QSet<QString>& expandedNodeIds) {
            eventExpandedNodeIds_ = expandedNodeIds;
        },
        [this](const QString& rawId) {
            auto* scenario = currentScenario();
            if (scenario == nullptr || rawId.isEmpty()) {
                return;
            }

            const auto id = rawId.section('/', 0, 0);
            const bool deletingSelectedItem = inspectorSelectionId_ == id;
            const auto beforeChange = currentOperationalEventHistoryEntry(id);
            if (canvas_ != nullptr && canvas_->deleteConnectionBlockById(id)) {
                if (deletingSelectedItem) {
                    setInspectorSelectionNone();
                }
                return;
            }
            if (canvas_ != nullptr && canvas_->deleteRouteGuidanceById(id)) {
                if (deletingSelectedItem) {
                    setInspectorSelectionNone();
                }
                return;
            }

            auto& hazards = scenario->draft.environment.hazards;
            const auto hazardId = id.toStdString();
            const auto hazardIt = std::remove_if(hazards.begin(), hazards.end(), [&](const auto& hazard) {
                return hazard.id == hazardId;
            });
            if (hazardIt != hazards.end()) {
                hazards.erase(hazardIt, hazards.end());
                if (beforeChange.has_value()) {
                    pushOperationalEventUndoEntry(*beforeChange);
                }
                if (canvas_ != nullptr) {
                    canvas_->setEnvironmentHazards(hazards);
                }
                recomputeDiffKeysAfterScenarioChanged(*scenario);
                if (deletingSelectedItem) {
                    setInspectorSelectionNone();
                }
                refreshNavigationPanel();
                refreshInspector();
                return;
            }

            const auto eventId = id.toStdString();
            auto& events = scenario->events;
            const auto it = std::remove_if(events.begin(), events.end(), [&](const auto& event) {
                return event.id == eventId;
            });
            if (it == events.end()) {
                return;
            }
            events.erase(it, events.end());
            if (beforeChange.has_value()) {
                pushOperationalEventUndoEntry(*beforeChange);
            }
            synchronizeOperationalEvents(*scenario);
            if (deletingSelectedItem) {
                setInspectorSelectionNone();
            } else {
                restoreOperationalEventSelection(selectedEventElementId_);
            }
            refreshNavigationPanel();
            refreshInspector();
        },
        [this](const QString& rawId) {
            if (canvas_ == nullptr || rawId.isEmpty()) {
                return;
            }

            const auto id = rawId.section('/', 0, 0);
            if (canvas_->editConnectionBlockScheduleById(id)) {
                return;
            }
            if (canvas_->editRouteGuidanceById(id)) {
                return;
            }

            auto* scenario = currentScenario();
            if (scenario == nullptr) {
                return;
            }

            auto& hazards = scenario->draft.environment.hazards;
            const auto hazardId = id.toStdString();
            const auto hazardIt = std::find_if(hazards.begin(), hazards.end(), [&](auto& hazard) {
                return hazard.id == hazardId;
            });
            if (hazardIt != hazards.end()) {
                const auto beforeChange = currentOperationalEventHistoryEntry(id);
                const auto previousHazard = *hazardIt;
                if (!editEnvironmentHazard(&(*hazardIt), layout_, this)) {
                    return;
                }
                if (environmentHazardEqual(*hazardIt, previousHazard)) {
                    return;
                }
                if (beforeChange.has_value()) {
                    pushOperationalEventUndoEntry(*beforeChange);
                }
                if (canvas_ != nullptr) {
                    canvas_->setEnvironmentHazards(hazards);
                }
                recomputeDiffKeysAfterScenarioChanged(*scenario);
                restoreOperationalEventSelection(id);
                refreshNavigationPanel();
                refreshInspector();
                return;
            }

            const auto eventId = id.toStdString();
            auto& events = scenario->events;
            const auto it = std::find_if(events.begin(), events.end(), [&](auto& event) {
                return event.id == eventId;
            });
            if (it == events.end()) {
                return;
            }
            const auto beforeChange = currentOperationalEventHistoryEntry(id);
            const auto previousEvent = *it;
            if (!editOperationalEvent(&(*it), this)) {
                return;
            }
            if (it->name == previousEvent.name
                && it->triggerSummary == previousEvent.triggerSummary
                && it->targetSummary == previousEvent.targetSummary) {
                return;
            }
            if (beforeChange.has_value()) {
                pushOperationalEventUndoEntry(*beforeChange);
            }
            synchronizeOperationalEvents(*scenario);
            restoreOperationalEventSelection(id);
            refreshNavigationPanel();
            refreshInspector();
        }));
}

void ScenarioAuthoringWidget::refreshRightPanel() {
    scenarioSwitcher_ = nullptr;
    elementInspectorPanel_ = nullptr;
    scenarioOverviewPanel_ = nullptr;
    scenarioDiffPanel_ = nullptr;
    newScenarioButton_ = nullptr;
    stageScenarioButton_ = nullptr;
    stagedScenariosLabel_ = nullptr;
    executeRunButton_ = nullptr;

    if (shell_ == nullptr) {
        return;
    }

    refreshPanelToggles();
    if (!inspectorPanelVisible_ && !scenarioPanelVisible_) {
        shell_->setReviewPanelVisible(false);
        return;
    }

    const int panelCount = (inspectorPanelVisible_ ? 1 : 0) + (scenarioPanelVisible_ ? 1 : 0);
    shell_->setReviewPanelWidth(panelCount > 1 ? 560 : 280);
    shell_->setReviewPanel(createRightPanelContainer());
    shell_->setReviewPanelVisible(true);
    refreshScenarioSwitcher();
    refreshInspector();
}

void ScenarioAuthoringWidget::refreshScenarioSwitcher() {
    if (scenarioSwitcher_ == nullptr) {
        return;
    }

    scenarioSwitcher_->blockSignals(true);
    scenarioSwitcher_->clear();
    for (const auto& scenario : scenarios_) {
        scenarioSwitcher_->addItem(QString("%1  (%2)")
            .arg(QString::fromStdString(scenario.draft.name), scenarioRoleLabel(scenario.draft.role)));
    }
    scenarioSwitcher_->setCurrentIndex(currentScenarioIndex_);
    scenarioSwitcher_->blockSignals(false);
}

void ScenarioAuthoringWidget::setInspectorSelectionNone() {
    inspectorSelectionKind_ = InspectorSelectionKind::None;
    inspectorSelectionId_.clear();
    selectedEventElementId_.clear();
}

void ScenarioAuthoringWidget::setInspectorSelectionFromCanvas(const ScenarioCanvasSelection& selection) {
    const auto rootId = selection.id.section('/', 0, 0);
    switch (selection.kind) {
    case ScenarioCanvasSelectionKind::LayoutElement:
        selectedLayoutElementId_ = selection.id;
        selectedCrowdElementId_.clear();
        selectedEventElementId_.clear();
        inspectorSelectionKind_ = selection.id.isEmpty() ? InspectorSelectionKind::None : InspectorSelectionKind::Layout;
        inspectorSelectionId_ = selection.id;
        if (!selection.id.isEmpty() && navigationView_ != NavigationView::Layout) {
            navigationView_ = NavigationView::Layout;
            refreshNavigationPanel();
        }
        break;
    case ScenarioCanvasSelectionKind::CrowdPlacement:
        selectedCrowdElementId_ = selection.id;
        selectedLayoutElementId_.clear();
        selectedEventElementId_.clear();
        inspectorSelectionKind_ = rootId.isEmpty() ? InspectorSelectionKind::None : InspectorSelectionKind::Crowd;
        inspectorSelectionId_ = rootId;
        if (!rootId.isEmpty() && navigationView_ != NavigationView::Crowd) {
            navigationView_ = NavigationView::Crowd;
            refreshNavigationPanel();
        }
        break;
    case ScenarioCanvasSelectionKind::ConnectionBlock:
        selectedLayoutElementId_.clear();
        selectedCrowdElementId_.clear();
        selectedEventElementId_ = rootId;
        inspectorSelectionKind_ = rootId.isEmpty() ? InspectorSelectionKind::None : InspectorSelectionKind::ConnectionBlock;
        inspectorSelectionId_ = rootId;
        if (!rootId.isEmpty() && navigationView_ != NavigationView::Events) {
            navigationView_ = NavigationView::Events;
            refreshNavigationPanel();
        } else if (navigationView_ == NavigationView::Events) {
            refreshNavigationPanel();
        }
        break;
    case ScenarioCanvasSelectionKind::EnvironmentHazard:
        selectedLayoutElementId_.clear();
        selectedCrowdElementId_.clear();
        selectedEventElementId_ = rootId;
        inspectorSelectionKind_ = rootId.isEmpty() ? InspectorSelectionKind::None : InspectorSelectionKind::EnvironmentHazard;
        inspectorSelectionId_ = rootId;
        if (!rootId.isEmpty() && navigationView_ != NavigationView::Events) {
            navigationView_ = NavigationView::Events;
            refreshNavigationPanel();
        } else if (navigationView_ == NavigationView::Events) {
            refreshNavigationPanel();
        }
        break;
    case ScenarioCanvasSelectionKind::RouteGuidance:
        selectedLayoutElementId_.clear();
        selectedCrowdElementId_.clear();
        selectedEventElementId_ = rootId;
        inspectorSelectionKind_ = rootId.isEmpty() ? InspectorSelectionKind::None : InspectorSelectionKind::RouteGuidance;
        inspectorSelectionId_ = rootId;
        if (!rootId.isEmpty() && navigationView_ != NavigationView::Events) {
            navigationView_ = NavigationView::Events;
            refreshNavigationPanel();
        } else if (navigationView_ == NavigationView::Events) {
            refreshNavigationPanel();
        }
        break;
    case ScenarioCanvasSelectionKind::None:
    default:
        setInspectorSelectionNone();
        break;
    }
    refreshInspector();
}

void ScenarioAuthoringWidget::setInspectorSelectionFromEventId(const QString& rawId) {
    const auto* scenario = currentScenario();
    const auto rootId = rawId.section('/', 0, 0);
    selectedLayoutElementId_.clear();
    selectedCrowdElementId_.clear();
    selectedEventElementId_ = rawId;
    inspectorSelectionId_ = rootId;
    inspectorSelectionKind_ = InspectorSelectionKind::None;

    if (scenario != nullptr && !rootId.isEmpty()) {
        const auto rootIdStd = rootId.toStdString();
        if (std::any_of(scenario->events.begin(), scenario->events.end(), [&](const auto& event) {
                return event.id == rootIdStd;
            })) {
            inspectorSelectionKind_ = InspectorSelectionKind::OperationalEvent;
        } else if (std::any_of(scenario->draft.environment.hazards.begin(), scenario->draft.environment.hazards.end(), [&](const auto& hazard) {
                return hazard.id == rootIdStd;
            })) {
            inspectorSelectionKind_ = InspectorSelectionKind::EnvironmentHazard;
        } else if (std::any_of(scenario->draft.control.routeGuidances.begin(), scenario->draft.control.routeGuidances.end(), [&](const auto& guidance) {
                return guidance.id == rootIdStd;
            })) {
            inspectorSelectionKind_ = InspectorSelectionKind::RouteGuidance;
        } else if (std::any_of(scenario->draft.control.connectionBlocks.begin(), scenario->draft.control.connectionBlocks.end(), [&](const auto& block) {
                return block.id == rootIdStd;
            })) {
            inspectorSelectionKind_ = InspectorSelectionKind::ConnectionBlock;
        }
    }

    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::runStagedScenarios() {
    std::vector<std::size_t> stagedIndexes;
    for (std::size_t index = 0; index < scenarios_.size(); ++index) {
        const auto& scenario = scenarios_[index];
        if (scenario.stagedForRun && scenarioHasOccupants(scenario)) {
            stagedIndexes.push_back(index);
        }
    }
    if (stagedIndexes.empty()) {
        if (stagedScenariosLabel_ != nullptr) {
            stagedScenariosLabel_->setText(stagedScenariosLabel_->text()
                + "\n\nNo staged scenario is ready to run.");
        }
        return;
    }

    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    if (!editRunSettingsForStagedScenarios(&scenarios_, stagedIndexes, this)) {
        return;
    }
    for (const auto index : stagedIndexes) {
        if (index < scenarios_.size()) {
            recomputeDiffKeysAfterScenarioChanged(scenarios_[index]);
        }
    }

    auto scenarios = stagedRunnableScenarios();
    auto* runWidget = new ScenarioRunWidget(
        projectName_,
        layout_,
        std::move(scenarios),
        saveProjectHandler_,
        openProjectHandler_,
        backToLayoutReviewHandler_,
        currentInitialState(),
        this);
    rootLayout->replaceWidget(shell_, runWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
}

void ScenarioAuthoringWidget::stageCurrentScenario() {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    if (!scenario->stagedForRun && !scenarioHasOccupants(*scenario)) {
        QMessageBox::warning(
            this,
            "Cannot stage scenario",
            "Add at least one occupant before staging this scenario.");
        refreshInspector();
        return;
    }

    scenario->stagedForRun = !scenario->stagedForRun;
    refreshInspector();
}

void ScenarioAuthoringWidget::updateCurrentScenarioPlacements(
    const std::vector<ScenarioCrowdPlacement>& placements,
    std::optional<CrowdPlacementHistoryEntry> beforeChange,
    const QString& selectedCrowdId) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    const auto resolvedSelectedCrowdId = selectedCrowdId.isEmpty()
        ? selectedCrowdElementId_.section('/', 0, 0)
        : selectedCrowdId.section('/', 0, 0);
    const bool changed = !crowdPlacementsEqual(scenario->crowdPlacements, placements);
    if (!beforeChange.has_value() && changed) {
        beforeChange = currentCrowdPlacementHistoryEntry(resolvedSelectedCrowdId);
    }

    scenario->crowdPlacements = placements;
    if (changed && beforeChange.has_value()) {
        pushCrowdPlacementUndoEntry(*beforeChange);
    }
    synchronizeCrowdPlacements(*scenario);
    if (changed) {
        restoreCrowdPlacementSelection(resolvedSelectedCrowdId);
    }
    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::recomputeDiffKeysAfterScenarioChanged(ScenarioState& scenario) {
    recomputeVariationDiffKeysIfAlternative(scenario);
    if (scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline) {
        recomputeDependentVariationDiffKeys(QString::fromStdString(scenario.draft.scenarioId));
    }
}

void ScenarioAuthoringWidget::recomputeDependentVariationDiffKeys(const QString& baselineId) {
    if (baselineId.isEmpty()) {
        return;
    }
    for (auto& scenario : scenarios_) {
        if (scenario.baseScenarioId == baselineId) {
            recomputeVariationDiffKeysIfAlternative(scenario);
        }
    }
}

void ScenarioAuthoringWidget::recomputeVariationDiffKeysIfAlternative(ScenarioState& scenario) const {
    if (!scenarioRoleHasBaselineDiff(scenario.draft.role) || scenario.baseScenarioId.isEmpty()) {
        scenario.draft.variationDiffKeys.clear();
        return;
    }
    const auto baseId = scenario.baseScenarioId.toStdString();
    for (const auto& candidate : scenarios_) {
        if (candidate.draft.scenarioId == baseId) {
            scenario.draft.variationDiffKeys =
                safecrowd::domain::computeScenarioDiffKeys(candidate.draft, scenario.draft);
            return;
        }
    }
    scenario.draft.variationDiffKeys.clear();
}

void ScenarioAuthoringWidget::showEmptyCanvas() {
    auto* canvas = new QWidget(shell_);
    canvas->setStyleSheet("QWidget { background: #f4f7fb; }");
    auto* layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);
    layout->addStretch(1);

    auto* title = createLabel("Create a scenario", canvas, ui::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto* detail = createLabel("Name the first scenario to start authoring Layout, Occupant, and Events settings.", canvas);
    detail->setAlignment(Qt::AlignCenter);
    detail->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(detail);

    auto* button = new QPushButton("New Scenario", canvas);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setStyleSheet(ui::primaryButtonStyleSheet());
    layout->addWidget(button, 0, Qt::AlignCenter);
    layout->addStretch(1);
    connect(button, &QPushButton::clicked, this, [this]() {
        showScenarioNameDialog(currentScenarioIndex_);
    });
    shell_->setCanvas(canvas);
}

void ScenarioAuthoringWidget::showScenarioNameDialog(int sourceIndex) {
    bool ok = false;
    const auto defaultName = sourceIndex >= 0
        ? QString("%1 alternative").arg(QString::fromStdString(scenarios_[sourceIndex].draft.name))
        : QString("Baseline evacuation");
    const auto name = QInputDialog::getText(
        this,
        "New Scenario",
        "Scenario name",
        QLineEdit::Normal,
        defaultName,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        if (scenarios_.empty()) {
            refreshInspector();
            showEmptyCanvas();
        }
        return;
    }

    createScenarioWithName(name, sourceIndex);
}

QWidget* ScenarioAuthoringWidget::createPanelToggleBar() {
    auto* bar = new QWidget(shell_);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    const auto buttonStyle = QString(
        "QPushButton {"
        " background: #ffffff;"
        " border: 0;"
        " border-radius: 8px;"
        " margin: 1px 0px;"
        " outline: none;"
        " padding: 4px;"
        "}"
        "QPushButton:hover {"
        " background: #eef3f8;"
        "}"
        "QPushButton:checked {"
        " background: #e6eef8;"
        "}"
        "QPushButton:focus {"
        " outline: none;"
        "}");

    inspectorPanelToggleButton_ = new QPushButton(bar);
    inspectorPanelToggleButton_->setCheckable(true);
    inspectorPanelToggleButton_->setChecked(inspectorPanelVisible_);
    inspectorPanelToggleButton_->setIcon(makeSvgToolIcon(
        QStringLiteral(":/tool-icons/etc/inspector-panel.svg"),
        QColor("#16202b"),
        QSize(22, 22)));
    inspectorPanelToggleButton_->setIconSize(QSize(22, 22));
    inspectorPanelToggleButton_->setFixedSize(36, 32);
    inspectorPanelToggleButton_->setCursor(Qt::PointingHandCursor);
    inspectorPanelToggleButton_->setFocusPolicy(Qt::NoFocus);
    inspectorPanelToggleButton_->setToolTip("Inspector");
    inspectorPanelToggleButton_->setAccessibleName("Toggle Inspector panel");
    inspectorPanelToggleButton_->setStyleSheet(buttonStyle);
    layout->addWidget(inspectorPanelToggleButton_);

    scenarioPanelToggleButton_ = new QPushButton(bar);
    scenarioPanelToggleButton_->setCheckable(true);
    scenarioPanelToggleButton_->setChecked(scenarioPanelVisible_);
    scenarioPanelToggleButton_->setIcon(makeSvgToolIcon(
        QStringLiteral(":/tool-icons/scenario-authoring/scenario-panel.svg"),
        QColor("#16202b"),
        QSize(22, 22)));
    scenarioPanelToggleButton_->setIconSize(QSize(22, 22));
    scenarioPanelToggleButton_->setFixedSize(36, 32);
    scenarioPanelToggleButton_->setCursor(Qt::PointingHandCursor);
    scenarioPanelToggleButton_->setFocusPolicy(Qt::NoFocus);
    scenarioPanelToggleButton_->setToolTip("Scenario");
    scenarioPanelToggleButton_->setAccessibleName("Toggle Scenario panel");
    scenarioPanelToggleButton_->setStyleSheet(buttonStyle);
    layout->addWidget(scenarioPanelToggleButton_);

    connect(inspectorPanelToggleButton_, &QPushButton::clicked, this, [this]() {
        inspectorPanelVisible_ = !inspectorPanelVisible_;
        refreshRightPanel();
    });
    connect(scenarioPanelToggleButton_, &QPushButton::clicked, this, [this]() {
        scenarioPanelVisible_ = !scenarioPanelVisible_;
        refreshRightPanel();
    });

    return bar;
}

void ScenarioAuthoringWidget::refreshPanelToggles() {
    if (inspectorPanelToggleButton_ != nullptr) {
        inspectorPanelToggleButton_->blockSignals(true);
        inspectorPanelToggleButton_->setChecked(inspectorPanelVisible_);
        inspectorPanelToggleButton_->blockSignals(false);
    }
    if (scenarioPanelToggleButton_ != nullptr) {
        scenarioPanelToggleButton_->blockSignals(true);
        scenarioPanelToggleButton_->setChecked(scenarioPanelVisible_);
        scenarioPanelToggleButton_->blockSignals(false);
    }
}

QWidget* ScenarioAuthoringWidget::createRightPanelContainer() {
    auto* container = new QWidget(shell_);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    if (inspectorPanelVisible_) {
        layout->addWidget(createElementInspectorPanel(), 1);
    }
    if (inspectorPanelVisible_ && scenarioPanelVisible_) {
        auto* separator = new QFrame(container);
        separator->setFrameShape(QFrame::NoFrame);
        separator->setFixedWidth(1);
        separator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        separator->setStyleSheet("QFrame { background: #c9d5e2; border: 0; min-width: 1px; max-width: 1px; }");
        layout->addWidget(separator);
    }
    if (scenarioPanelVisible_) {
        layout->addWidget(createScenarioPanel(), 1);
    }
    return container;
}

QWidget* ScenarioAuthoringWidget::createElementInspectorPanel() {
    auto* inspector = new QWidget(shell_);
    inspector->setMinimumWidth(0);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    inspectorLayout->setSpacing(12);
    inspectorLayout->addWidget(createLabel("Inspector", inspector, ui::FontRole::Title));

    auto* card = createInspectorCard(inspector);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(card);
    scrollArea->setMinimumWidth(0);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: 0; }");
    elementInspectorPanel_ = new QWidget(scrollArea);
    elementInspectorPanel_->setMinimumWidth(0);
    elementInspectorPanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* contentLayout = new QVBoxLayout(elementInspectorPanel_);
    contentLayout->setContentsMargins(12, 11, 12, 11);
    contentLayout->setSpacing(8);
    scrollArea->setWidget(elementInspectorPanel_);
    cardLayout->addWidget(scrollArea);
    inspectorLayout->addWidget(card, 1);

    return inspector;
}

QWidget* ScenarioAuthoringWidget::createRunPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(createLabel("Run", panel, ui::FontRole::Title));

    stagedScenariosLabel_ = createLabel("", panel);
    stagedScenariosLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(stagedScenariosLabel_);

    executeRunButton_ = new QPushButton("Run Staged Scenarios", panel);
    executeRunButton_->setFont(ui::font(ui::FontRole::Body));
    executeRunButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    executeRunButton_->setEnabled(false);
    layout->addWidget(executeRunButton_);

    auto* editButton = new QPushButton("Edit Scenario", panel);
    editButton->setFont(ui::font(ui::FontRole::Body));
    editButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    layout->addWidget(editButton);
    layout->addStretch(1);

    connect(executeRunButton_, &QPushButton::clicked, this, [this]() {
        runStagedScenarios();
    });
    connect(editButton, &QPushButton::clicked, this, [this]() {
        scenarioPanelVisible_ = true;
        refreshRightPanel();
    });

    return panel;
}

QWidget* ScenarioAuthoringWidget::createScenarioPanel() {
    auto* inspector = new QWidget(shell_);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    inspectorLayout->setSpacing(12);
    inspectorLayout->addWidget(createLabel("Scenario", inspector, ui::FontRole::Title));

    scenarioSwitcher_ = new QComboBox(inspector);
    scenarioSwitcher_->setFont(ui::font(ui::FontRole::Body));
    scenarioSwitcher_->setStyleSheet(
        "QComboBox { background: #ffffff; border: 1px solid #c9d5e2; border-radius: 10px; padding: 8px 10px; min-height: 24px; }");
    inspectorLayout->addWidget(scenarioSwitcher_);

    newScenarioButton_ = new QPushButton("New Scenario from Current", inspector);
    newScenarioButton_->setFont(ui::font(ui::FontRole::Body));
    newScenarioButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
    inspectorLayout->addWidget(newScenarioButton_);

    auto* overviewCard = createInspectorCard(inspector);
    auto* overviewCardLayout = new QVBoxLayout(overviewCard);
    overviewCardLayout->setContentsMargins(0, 0, 0, 0);
    overviewCardLayout->setSpacing(0);

    auto* overviewScrollArea = new QScrollArea(overviewCard);
    overviewScrollArea->setWidgetResizable(true);
    overviewScrollArea->setFrameShape(QFrame::NoFrame);
    overviewScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    overviewScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    overviewScrollArea->setStyleSheet("QScrollArea { background: transparent; border: 0; }");
    overviewScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    overviewScrollArea->setMinimumHeight(150);
    scenarioOverviewPanel_ = new QWidget(overviewScrollArea);
    scenarioOverviewPanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* overviewLayout = new QVBoxLayout(scenarioOverviewPanel_);
    overviewLayout->setContentsMargins(12, 11, 12, 11);
    overviewLayout->setSpacing(8);
    overviewScrollArea->setWidget(scenarioOverviewPanel_);
    overviewCardLayout->addWidget(overviewScrollArea);
    inspectorLayout->addWidget(overviewCard, 3);

    auto* diffCard = createInspectorCard(inspector);
    auto* diffCardLayout = new QVBoxLayout(diffCard);
    diffCardLayout->setContentsMargins(0, 0, 0, 0);
    diffCardLayout->setSpacing(0);

    auto* diffScrollArea = new QScrollArea(diffCard);
    diffScrollArea->setWidgetResizable(true);
    diffScrollArea->setFrameShape(QFrame::NoFrame);
    diffScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    diffScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    diffScrollArea->setStyleSheet("QScrollArea { background: transparent; border: 0; }");
    diffScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    diffScrollArea->setMinimumHeight(110);
    scenarioDiffPanel_ = new QWidget(diffScrollArea);
    scenarioDiffPanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* diffLayout = new QVBoxLayout(scenarioDiffPanel_);
    diffLayout->setContentsMargins(10, 10, 10, 10);
    diffLayout->setSpacing(7);
    diffScrollArea->setWidget(scenarioDiffPanel_);
    diffCardLayout->addWidget(diffScrollArea);
    inspectorLayout->addWidget(diffCard, 2);

    stageScenarioButton_ = new QPushButton("Stage Scenario", inspector);
    stageScenarioButton_->setFont(ui::font(ui::FontRole::Body));
    inspectorLayout->addWidget(stageScenarioButton_);

    auto* separator = new QFrame(inspector);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("QFrame { color: #d7e0ea; background: #d7e0ea; max-height: 1px; }");
    inspectorLayout->addWidget(separator);

    inspectorLayout->addWidget(createLabel("Run", inspector, ui::FontRole::Title));

    stagedScenariosLabel_ = createLabel("", inspector);
    stagedScenariosLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    QStringList lines;
    const auto stagedCount = std::count_if(scenarios_.begin(), scenarios_.end(), [](const auto& scenario) {
        return scenario.stagedForRun && scenarioHasOccupants(scenario);
    });
    if (stagedCount == 0) {
        lines << "No staged scenarios";
    } else {
        lines << "Staged scenarios";
        for (const auto& scenario : scenarios_) {
            if (!scenario.stagedForRun || !scenarioHasOccupants(scenario)) {
                continue;
            }
            lines << QString("- %1 (%2)")
                .arg(QString::fromStdString(scenario.draft.name), scenarioRoleLabel(scenario.draft.role));
        }
    }
    stagedScenariosLabel_->setText(lines.join('\n'));
    inspectorLayout->addWidget(stagedScenariosLabel_);

    executeRunButton_ = new QPushButton("Run Staged Scenarios", inspector);
    executeRunButton_->setFont(ui::font(ui::FontRole::Body));
    executeRunButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    executeRunButton_->setEnabled(stagedCount > 0);
    inspectorLayout->addWidget(executeRunButton_);

    connect(scenarioSwitcher_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < static_cast<int>(scenarios_.size()) && index != currentScenarioIndex_) {
            currentScenarioIndex_ = index;
            selectedLayoutElementId_.clear();
            selectedCrowdElementId_.clear();
            setInspectorSelectionNone();
            refreshCanvas();
            refreshNavigationPanel();
            refreshInspector();
        }
    });
    connect(newScenarioButton_, &QPushButton::clicked, this, [this]() {
        createScenarioFromCurrent();
    });
    connect(stageScenarioButton_, &QPushButton::clicked, this, [this]() {
        stageCurrentScenario();
    });
    connect(executeRunButton_, &QPushButton::clicked, this, [this]() {
        runStagedScenarios();
    });

    return inspector;
}

ScenarioAuthoringWidget::ScenarioState* ScenarioAuthoringWidget::currentScenario() {
    if (currentScenarioIndex_ < 0 || currentScenarioIndex_ >= static_cast<int>(scenarios_.size())) {
        return nullptr;
    }
    return &scenarios_[currentScenarioIndex_];
}

const ScenarioAuthoringWidget::ScenarioState* ScenarioAuthoringWidget::currentScenario() const {
    if (currentScenarioIndex_ < 0 || currentScenarioIndex_ >= static_cast<int>(scenarios_.size())) {
        return nullptr;
    }
    return &scenarios_[currentScenarioIndex_];
}

std::vector<safecrowd::domain::ScenarioDraft> ScenarioAuthoringWidget::stagedRunnableScenarios() const {
    std::vector<safecrowd::domain::ScenarioDraft> staged;
    for (const auto& scenario : scenarios_) {
        if (scenario.stagedForRun && scenarioHasOccupants(scenario)) {
            auto draft = scenario.draft;
            draft.control.events = scenario.events;
            staged.push_back(std::move(draft));
        }
    }
    return staged;
}

}  // namespace safecrowd::application
