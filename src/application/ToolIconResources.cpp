#include "application/ToolIconResources.h"

#include <QByteArray>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

#include <algorithm>

namespace safecrowd::application {

QIcon makeSvgToolIcon(const QString& resourcePath, const QColor& color, QSize size) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    auto svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        return {};
    }

    constexpr double kDevicePixelRatio = 2.0;
    QPixmap pixmap(size * kDevicePixelRatio);
    pixmap.setDevicePixelRatio(kDevicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF target(0.5, 0.5, std::max(1, size.width() - 1), std::max(1, size.height() - 1));
    renderer.render(&painter, target);

    return QIcon(pixmap);
}

}  // namespace safecrowd::application
