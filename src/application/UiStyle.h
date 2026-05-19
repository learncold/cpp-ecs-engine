#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QWidget;

namespace safecrowd::application::ui {

enum class FontRole {
    Hero,
    Title,
    SectionTitle,
    Body,
    Caption,
    MonoCaption,
};

QFont font(FontRole role);
QString appStyleSheet();
QString panelStyleSheet();
QString cardStyleSheet();
QString tagStyleSheet(bool selected = false);
QString primaryButtonStyleSheet();
QString secondaryButtonStyleSheet();
QString textFieldStyleSheet(bool readOnly = false);
QString ghostRowStyleSheet();
QString severityTextStyleSheet(const QColor& color);
QString mutedTextStyleSheet();
QString subtleTextStyleSheet();
QString canvasSurfaceStyleSheet();
QString canvasToolbarStyleSheet();
QString layoutPreviewPropertyPanelStyleSheet();
QString scenarioCanvasPropertyPanelStyleSheet();
QString canvasInlineLabelStyleSheet();
QString simulationFloorSelectorStyleSheet();

void polishCanvasSurface(QWidget* widget);
void polishCanvasToolbar(QWidget* widget);
void polishLayoutPreviewPropertyPanel(QWidget* widget);
void polishScenarioCanvasPropertyPanel(QWidget* widget);
void polishCanvasInlineLabel(QWidget* widget);
void polishSimulationFloorSelector(QWidget* widget);
void polishScrollArea(QWidget* widget);

}  // namespace safecrowd::application::ui
