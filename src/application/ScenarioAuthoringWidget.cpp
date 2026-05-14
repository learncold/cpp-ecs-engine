#include "application/ScenarioAuthoringWidget.h"

#include "domain/GeometryQueries.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
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

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
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
    return QString("%1s - %2s").arg(hazard.startSeconds, 0, 'f', 1).arg(hazard.endSeconds, 0, 'f', 1);
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

    auto* kindCombo = new QComboBox(&dialog);
    kindCombo->addItem("Fire", static_cast<int>(safecrowd::domain::EnvironmentHazardKind::Fire));
    kindCombo->addItem("Smoke", static_cast<int>(safecrowd::domain::EnvironmentHazardKind::Smoke));
    kindCombo->setCurrentIndex(std::max(0, kindCombo->findData(static_cast<int>(hazard->kind))));

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
    startSpin->setValue(std::max(0.0, hazard->startSeconds));

    auto* endSpin = new QDoubleSpinBox(&dialog);
    endSpin->setRange(0.0, 86400.0);
    endSpin->setDecimals(1);
    endSpin->setSuffix(" s");
    endSpin->setValue(std::max(0.0, hazard->endSeconds));

    auto* severityCombo = new QComboBox(&dialog);
    severityCombo->addItem("Low", static_cast<int>(safecrowd::domain::ScenarioElementSeverity::Low));
    severityCombo->addItem("Medium", static_cast<int>(safecrowd::domain::ScenarioElementSeverity::Medium));
    severityCombo->addItem("High", static_cast<int>(safecrowd::domain::ScenarioElementSeverity::High));
    severityCombo->setCurrentIndex(std::max(0, severityCombo->findData(static_cast<int>(hazard->severity))));

    auto* noteEdit = new QPlainTextEdit(&dialog);
    noteEdit->setPlainText(QString::fromStdString(hazard->note));
    noteEdit->setMinimumHeight(72);

    form->addRow("Kind", kindCombo);
    form->addRow("Name", nameEdit);
    form->addRow("Affected zone", zoneCombo);
    form->addRow("X", xSpin);
    form->addRow("Y", ySpin);
    form->addRow("Start", startSpin);
    form->addRow("End", endSpin);
    form->addRow("Severity", severityCombo);
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

    hazard->kind = static_cast<safecrowd::domain::EnvironmentHazardKind>(kindCombo->currentData().toInt());
    hazard->name = name.toStdString();
    hazard->affectedZoneId = selectedZoneId;
    hazard->floorId = selectedZone->floorId;
    hazard->position = selectedPosition;
    hazard->startSeconds = startSpin->value();
    hazard->endSeconds = std::max(hazard->startSeconds, endSpin->value());
    hazard->severity = static_cast<safecrowd::domain::ScenarioElementSeverity>(severityCombo->currentData().toInt());
    hazard->note = noteEdit->toPlainText().trimmed().toStdString();
    return true;
}

int draftOccupantCount(const safecrowd::domain::ScenarioDraft& scenario) {
    int total = 0;
    for (const auto& placement : scenario.population.initialPlacements) {
        total += static_cast<int>(placement.targetAgentCount);
    }
    return total;
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
        const auto baselinePlacements = static_cast<int>(baseline.population.initialPlacements.size());
        const auto variantPlacements = static_cast<int>(variant.population.initialPlacements.size());
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
    return QString::fromStdString(key);
}

QString changeCategoryLabel(const std::string& key) {
    if (key.rfind("population.", 0) == 0) {
        return "Crowd";
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
        if (auto* widget = item->widget()) {
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

QIcon makeCrowdIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QPointF(22, 12), 5.5, 5.5);
    painter.drawEllipse(QPointF(14, 18), 4.6, 4.6);
    painter.drawEllipse(QPointF(30, 18), 4.6, 4.6);
    painter.drawRoundedRect(QRectF(12, 24, 20, 8), 4, 4);
    painter.drawRoundedRect(QRectF(6, 28, 16, 7), 3.5, 3.5);
    painter.drawRoundedRect(QRectF(22, 28, 16, 7), 3.5, 3.5);
    return QIcon(pixmap);
}

QIcon makeEventsIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(13, 31), QPointF(22, 12));
    painter.drawLine(QPointF(22, 12), QPointF(31, 31));
    painter.drawLine(QPointF(16, 25), QPointF(28, 25));
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QPointF(22, 12), 4, 4);
    painter.drawEllipse(QPointF(13, 31), 4, 4);
    painter.drawEllipse(QPointF(31, 31), 4, 4);
    return QIcon(pixmap);
}

QIcon makeLayoutIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(11, 10, 9, 10));
    painter.drawRect(QRectF(24, 10, 9, 10));
    painter.drawRect(QRectF(11, 24, 22, 10));
    painter.drawLine(QPointF(20, 15), QPointF(24, 15));
    painter.drawLine(QPointF(22, 20), QPointF(22, 24));
    return QIcon(pixmap);
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

std::vector<NavigationTreeNode> buildCrowdTree(const ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr || scenario->crowdPlacements.empty()) {
        return {};
    }

    std::vector<NavigationTreeNode> placements;
    for (const auto& placement : scenario->crowdPlacements) {
        const bool group = placement.kind == ScenarioCrowdPlacementKind::Group;
        std::vector<NavigationTreeNode> occupants;
        for (int index = 1; index <= placement.occupantCount; ++index) {
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

        placements.push_back({
            .label = QString("%1  -  %2  -  %3 %4")
                         .arg(
                             placement.name.isEmpty() ? placement.id : placement.name,
                             placement.zoneId)
                         .arg(placement.occupantCount)
                         .arg(placement.occupantCount == 1 ? "occupant" : "occupants"),
            .id = placement.id,
            .detail = QString("Velocity: (%1, %2)")
                          .arg(placement.velocity.x, 0, 'f', 2)
                          .arg(placement.velocity.y, 0, 'f', 2),
            .icon = group ? groupCrowdTreeIcon() : individualCrowdTreeIcon(),
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
        "Crowd",
        buildCrowdTree(scenario),
        "No pedestrian placements yet",
        std::move(selectPlacementHandler),
        parent,
        shell != nullptr ? shell->createPanelHeader("Crowd", parent, false) : nullptr,
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
            QStringList details;
            details << QString("Zone: %1").arg(zone)
                    << QString("Location: %1").arg(position)
                    << QString("Period: %1").arg(schedule)
                    << QString("Severity: %1").arg(severity);
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
            const auto doorLabel = guidance.installConnectionId.empty()
                ? QString{}
                : connectionLabelForId(layout, guidance.installConnectionId);
            const auto exitLabel = guidance.guidedExitZoneId.empty()
                ? QStringLiteral("Nearest exit")
                : zoneName(layout, guidance.guidedExitZoneId);
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
            children.reserve(doorLabel.isEmpty() ? 2u : 3u);
            children.push_back({
                .label = QString("Exit  -  %1").arg(exitLabel),
                .id = QString("%1/exit").arg(guidanceId),
            });
            if (!doorLabel.isEmpty()) {
                children.push_back({
                    .label = QString("Door  -  %1").arg(doorLabel),
                    .id = QString("%1/door").arg(guidanceId),
                });
            }
            children.push_back({
                .label = QString("Period  -  %1").arg(periodSummary),
                .id = QString("%1/period").arg(guidanceId),
            });

            nodes.push_back({
                .label = QString("Guidance  -  %1").arg(doorLabel.isEmpty() ? exitLabel : doorLabel),
                .id = guidanceId,
                .detail = QString("Period: %1").arg(periodSummary),
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
    std::function<void(const QString&)> deleteItemHandler,
    std::function<void(const QString&)> settingsItemHandler) {
    return new NavigationTreeWidget(
        "Events / Hazards",
        buildEventsTree(layout, scenario),
        "No operational events, hazards, or blocked exits yet",
        {},
        parent,
        shell != nullptr ? shell->createPanelHeader("Events / Hazards", parent, false) : nullptr,
        {},
        {},
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
      rightPanelMode_(initialState.rightPanelMode) {
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

    shell_ = new WorkspaceShell(this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler(backToLayoutReviewHandler_);
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
    switch (rightPanelMode_) {
    case RightPanelMode::None:
        state.rightPanelMode = SavedRightPanelMode::None;
        break;
    case RightPanelMode::Run:
        state.rightPanelMode = SavedRightPanelMode::Run;
        break;
    case RightPanelMode::Scenario:
    default:
        state.rightPanelMode = SavedRightPanelMode::Scenario;
        break;
    }
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
    state.rightPanelMode = rightPanelMode_;
    state.scenarios.reserve(scenarios_.size());
    for (const auto& scenario : scenarios_) {
        auto copy = scenario;
        copy.draft.control.events = copy.events;
        copy.stagedForRun = copy.stagedForRun && scenarioHasOccupants(copy);
        state.scenarios.push_back(std::move(copy));
    }
    return state;
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

    scenario->events.push_back({
        .id = QString("event-%1").arg(static_cast<int>(scenario->events.size()) + 1).toStdString(),
        .name = name.toStdString(),
        .triggerSummary = trigger.toStdString(),
        .targetSummary = target.toStdString(),
    });
    scenario->draft.control.events = scenario->events;
    recomputeDiffKeysAfterScenarioChanged(*scenario);
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
        scenario.baseScenarioId = source.draft.role == safecrowd::domain::ScenarioRole::Alternative
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
    canvas_->setLayoutElementActivatedHandler([this](const QString& elementId) {
        selectedLayoutElementId_ = elementId;
        if (!elementId.isEmpty()) {
            selectedCrowdElementId_.clear();
            if (navigationView_ != NavigationView::Layout) {
                navigationView_ = NavigationView::Layout;
                refreshNavigationPanel();
                return;
            }
        }
        if (navigationView_ == NavigationView::Layout) {
            refreshNavigationPanel();
        }
    });
    canvas_->setCrowdSelectionChangedHandler([this](const QString& elementId) {
        selectedCrowdElementId_ = elementId;
        if (!elementId.isEmpty()) {
            selectedLayoutElementId_.clear();
            if (navigationView_ != NavigationView::Crowd) {
                navigationView_ = NavigationView::Crowd;
                refreshNavigationPanel();
                return;
            }
        }
        if (navigationView_ == NavigationView::Crowd) {
            refreshNavigationPanel();
        }
    });
    canvas_->setConnectionBlocks(scenario->draft.control.connectionBlocks);
    canvas_->setConnectionBlocksChangedHandler([this](const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        current->draft.control.connectionBlocks = blocks;
        recomputeDiffKeysAfterScenarioChanged(*current);
        refreshNavigationPanel();
        refreshInspector();
    });
    canvas_->setEnvironmentHazards(scenario->draft.environment.hazards);
    canvas_->setEnvironmentHazardsChangedHandler([this](const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        current->draft.environment.hazards = hazards;
        recomputeDiffKeysAfterScenarioChanged(*current);
        refreshNavigationPanel();
        refreshInspector();
    });
    canvas_->setRouteGuidances(scenario->draft.control.routeGuidances);
    canvas_->setRouteGuidancesChangedHandler([this](const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        current->draft.control.routeGuidances = guidances;
        recomputeDiffKeysAfterScenarioChanged(*current);
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
                const bool alternative = scenario->draft.role == safecrowd::domain::ScenarioRole::Alternative;
                panelLayout->addWidget(createRoleBadge(
                    alternative ? "Alternative" : "Baseline",
                    alternative,
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
                if (alternative && !scenario->baseScenarioId.isEmpty()) {
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
                addStatusMessage(panelLayout, "Alternative scenario / no baseline link", scenarioDiffPanel_);
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
                const auto role = stagedScenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
                lines << QString("- %1 (%2)").arg(QString::fromStdString(stagedScenario.draft.name), role);
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
                .icon = makeLayoutIcon(QColor("#1f5fae")),
            },
            {
                .id = "crowd",
                .label = "Crowd",
                .icon = makeCrowdIcon(QColor("#1f5fae")),
            },
            {
                .id = "events",
                .label = "Events / Hazards",
                .icon = makeEventsIcon(QColor("#1f5fae")),
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
                if (canvas_ != nullptr) {
                    canvas_->activateLayoutElement(elementId);
                }
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
                if (canvas_ != nullptr) {
                    canvas_->focusPlacement(placementId);
                }
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
            auto* scenario = currentScenario();
            if (scenario == nullptr || rawId.isEmpty()) {
                return;
            }

            const auto id = rawId.section('/', 0, 0);
            if (canvas_ != nullptr && canvas_->deleteConnectionBlockById(id)) {
                return;
            }
            if (canvas_ != nullptr && canvas_->deleteRouteGuidanceById(id)) {
                return;
            }

            auto& hazards = scenario->draft.environment.hazards;
            const auto hazardId = id.toStdString();
            const auto hazardIt = std::remove_if(hazards.begin(), hazards.end(), [&](const auto& hazard) {
                return hazard.id == hazardId;
            });
            if (hazardIt != hazards.end()) {
                hazards.erase(hazardIt, hazards.end());
                if (canvas_ != nullptr) {
                    canvas_->setEnvironmentHazards(hazards);
                }
                recomputeDiffKeysAfterScenarioChanged(*scenario);
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
            scenario->draft.control.events = scenario->events;
            recomputeDiffKeysAfterScenarioChanged(*scenario);
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
                if (!editEnvironmentHazard(&(*hazardIt), layout_, this)) {
                    return;
                }
                if (canvas_ != nullptr) {
                    canvas_->setEnvironmentHazards(hazards);
                }
                recomputeDiffKeysAfterScenarioChanged(*scenario);
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
            if (!editOperationalEvent(&(*it), this)) {
                return;
            }
            scenario->draft.control.events = scenario->events;
            recomputeDiffKeysAfterScenarioChanged(*scenario);
            refreshNavigationPanel();
            refreshInspector();
        }));
}

void ScenarioAuthoringWidget::refreshRightPanel() {
    scenarioSwitcher_ = nullptr;
    scenarioOverviewPanel_ = nullptr;
    scenarioDiffPanel_ = nullptr;
    newScenarioButton_ = nullptr;
    stageScenarioButton_ = nullptr;
    stagedScenariosLabel_ = nullptr;
    executeRunButton_ = nullptr;

    if (shell_ == nullptr) {
        return;
    }

    if (rightPanelMode_ == RightPanelMode::None) {
        shell_->setReviewPanelVisible(false);
        return;
    }

    shell_->setReviewPanelVisible(true);
    if (rightPanelMode_ == RightPanelMode::Run) {
        shell_->setReviewPanel(createRunPanel());
    } else {
        rightPanelMode_ = RightPanelMode::Scenario;
        shell_->setReviewPanel(createScenarioPanel());
    }
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
        const auto role = scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
        scenarioSwitcher_->addItem(QString("%1  (%2)").arg(QString::fromStdString(scenario.draft.name), role));
    }
    scenarioSwitcher_->setCurrentIndex(currentScenarioIndex_);
    scenarioSwitcher_->blockSignals(false);
}

void ScenarioAuthoringWidget::runStagedScenarios() {
    auto scenarios = stagedRunnableScenarios();
    if (scenarios.empty()) {
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

void ScenarioAuthoringWidget::updateCurrentScenarioPlacements(const std::vector<ScenarioCrowdPlacement>& placements) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    scenario->crowdPlacements = placements;
    scenario->draft.population.initialPlacements.clear();
    for (const auto& placement : scenario->crowdPlacements) {
        safecrowd::domain::InitialPlacement2D initialPlacement;
        initialPlacement.id = placement.id.toStdString();
        initialPlacement.zoneId = placement.zoneId.toStdString();
        initialPlacement.floorId = placement.floorId.toStdString();
        initialPlacement.area.outline = placement.area;
        initialPlacement.targetAgentCount = static_cast<std::size_t>(placement.occupantCount);
        initialPlacement.initialVelocity = placement.velocity;
        initialPlacement.distribution = placement.distribution;
        initialPlacement.explicitPositions = placement.generatedPositions;
        scenario->draft.population.initialPlacements.push_back(std::move(initialPlacement));
    }
    if (!scenarioHasOccupants(*scenario)) {
        scenario->stagedForRun = false;
    }

    recomputeDiffKeysAfterScenarioChanged(*scenario);
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
    if (scenario.draft.role != safecrowd::domain::ScenarioRole::Alternative
        || scenario.baseScenarioId.isEmpty()) {
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
    auto* detail = createLabel("Name the first scenario to start authoring Layout, Crowd, and Events settings.", canvas);
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
        rightPanelMode_ = RightPanelMode::Scenario;
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
            const auto role = scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
            lines << QString("- %1 (%2)").arg(QString::fromStdString(scenario.draft.name), role);
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
