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

void polishScrollArea(QWidget* widget);

}  // namespace safecrowd::application::ui
