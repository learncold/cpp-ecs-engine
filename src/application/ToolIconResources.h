#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>

namespace safecrowd::application {

QIcon makeSvgToolIcon(const QString& resourcePath, const QColor& color, QSize size);

}  // namespace safecrowd::application
