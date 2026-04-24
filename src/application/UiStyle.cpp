#include "application/UiStyle.h"

#include <QScrollArea>
#include <QWidget>

namespace safecrowd::application::ui {
namespace {

constexpr auto kFontFamily = "Segoe UI";
constexpr auto kMonoFontFamily = "Consolas";
constexpr auto kTextPrimary = "#16202b";
constexpr auto kTextSecondary = "#4f5d6b";
constexpr auto kTextMuted = "#73808c";
constexpr auto kSurfaceBase = "#f4f7fb";
constexpr auto kSurfaceCard = "#ffffff";
constexpr auto kSurfaceHover = "#eef3f8";
constexpr auto kSurfaceAccent = "#e6eef8";
constexpr auto kBorderSoft = "#d7e0ea";
constexpr auto kBorderStrong = "#b8c6d6";
constexpr auto kPrimary = "#1f5fae";
constexpr auto kPrimaryHover = "#174d8f";

QFont makeFont(const QString& family, int pointSize, QFont::Weight weight, qreal letterSpacing = 0.0) {
    QFont font(family, pointSize, weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    if (letterSpacing != 0.0) {
        font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    }
    return font;
}

}  // namespace

QFont font(FontRole role) {
    switch (role) {
    case FontRole::Hero:
        return makeFont(kFontFamily, 28, QFont::DemiBold, 0.3);
    case FontRole::Title:
        return makeFont(kFontFamily, 20, QFont::DemiBold, 0.2);
    case FontRole::SectionTitle:
        return makeFont(kFontFamily, 12, QFont::DemiBold, 0.2);
    case FontRole::Body:
        return makeFont(kFontFamily, 10, QFont::Normal, 0.1);
    case FontRole::Caption:
        return makeFont(kFontFamily, 9, QFont::Medium, 0.1);
    case FontRole::MonoCaption:
        return makeFont(kMonoFontFamily, 9, QFont::Normal);
    }

    return makeFont(kFontFamily, 10, QFont::Normal);
}

QString appStyleSheet() {
    return QString(
        "QWidget {"
        " color: %1;"
        " background: transparent;"
        "}"
        "QMainWindow {"
        " background: %2;"
        "}"
        "QMenu {"
        " background: %3;"
        " border: 1px solid %4;"
        " padding: 8px;"
        "}"
        "QMenu::item {"
        " padding: 8px 12px;"
        " border-radius: 8px;"
        "}"
        "QMenu::item:selected {"
        " background: %5;"
        "}"
        "QScrollBar:vertical {"
        " background: transparent;"
        " width: 10px;"
        " margin: 4px 0;"
        "}"
        "QScrollBar::handle:vertical {"
        " background: %4;"
        " border-radius: 5px;"
        " min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        " background: transparent;"
        " height: 0px;"
        "}"
    ).arg(kTextPrimary, kSurfaceBase, kSurfaceCard, kBorderSoft, kSurfaceHover);
}

QString panelStyleSheet() {
    return QString(
        "QFrame {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 16px;"
        "}"
        "QLabel {"
        " background: transparent;"
        " border: 0;"
        "}"
    ).arg(kSurfaceCard, kBorderSoft);
}

QString cardStyleSheet() {
    return QString(
        "IssueCardWidget {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 14px;"
        "}"
        "IssueCardWidget:hover {"
        " background: %3;"
        " border-color: %4;"
        "}"
        "QLabel {"
        " background: transparent;"
        " border: 0;"
        "}"
    ).arg(kSurfaceCard, kBorderSoft, kSurfaceHover, kBorderStrong);
}

QString tagStyleSheet(bool selected) {
    const auto background = selected ? kSurfaceAccent : kSurfaceCard;
    const auto border = selected ? kPrimary : kBorderSoft;
    const auto text = selected ? kPrimary : kTextSecondary;
    const auto weight = selected ? "600" : "500";
    return QString(
        "QPushButton {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 12px;"
        " color: %3;"
        " font-weight: %4;"
        " padding: 9px 12px;"
        " text-align: left;"
        "}"
        "QPushButton:hover {"
        " background: %5;"
        "}"
        "QPushButton:checked {"
        " background: %6;"
        " border-color: %7;"
        " color: %7;"
        "}"
    ).arg(background, border, text, weight, kSurfaceHover, kSurfaceAccent, kPrimary);
}

QString primaryButtonStyleSheet() {
    return QString(
        "QPushButton {"
        " background: %1;"
        " border: 1px solid %1;"
        " border-radius: 12px;"
        " color: white;"
        " font-weight: 600;"
        " padding: 10px 18px;"
        "}"
        "QPushButton:hover {"
        " background: %2;"
        " border-color: %2;"
        "}"
        "QPushButton:disabled {"
        " background: #c3cfdb;"
        " border-color: #c3cfdb;"
        " color: #f7f9fb;"
        "}"
    ).arg(kPrimary, kPrimaryHover);
}

QString secondaryButtonStyleSheet() {
    return QString(
        "QPushButton {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 12px;"
        " color: %3;"
        " font-weight: 600;"
        " padding: 10px 18px;"
        "}"
        "QPushButton:hover {"
        " background: %4;"
        " border-color: %5;"
        "}"
        "QPushButton:pressed {"
        " background: %6;"
        "}"
    ).arg(kSurfaceCard, kBorderSoft, kTextPrimary, kSurfaceHover, kBorderStrong, kSurfaceAccent);
}

QString textFieldStyleSheet(bool readOnly) {
    const auto background = readOnly ? "#f8fafc" : kSurfaceCard;
    return QString(
        "QLineEdit {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 12px;"
        " color: %3;"
        " padding: 10px 12px;"
        " selection-background-color: %4;"
        "}"
        "QLineEdit:focus {"
        " border-color: %5;"
        "}"
    ).arg(background, kBorderSoft, kTextPrimary, kSurfaceAccent, kPrimary);
}

QString ghostRowStyleSheet() {
    return QString(
        "QPushButton {"
        " background: transparent;"
        " border: 1px solid transparent;"
        " border-radius: 14px;"
        " text-align: left;"
        " padding: 12px 14px;"
        "}"
        "QPushButton:hover {"
        " background: %1;"
        " border-color: %2;"
        "}"
    ).arg(kSurfaceHover, kBorderSoft);
}

QString severityTextStyleSheet(const QColor& color) {
    return QString("QLabel { color: %1; }").arg(color.name());
}

QString mutedTextStyleSheet() {
    return QString("QLabel { color: %1; }").arg(kTextSecondary);
}

QString subtleTextStyleSheet() {
    return QString("QLabel { color: %1; }").arg(kTextMuted);
}

void polishScrollArea(QWidget* widget) {
    if (auto* scrollArea = qobject_cast<QScrollArea*>(widget)) {
        scrollArea->setStyleSheet("QScrollArea { background: transparent; border: 0; }");
        if (scrollArea->viewport() != nullptr) {
            scrollArea->viewport()->setAutoFillBackground(false);
        }
    }
}

}  // namespace safecrowd::application::ui
