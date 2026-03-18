#include "application/MainWindow.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "domain/SafeCrowdDomain.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    const safecrowd::SafeCrowdDomain domain{};
    const safecrowd::Overview overview = domain.buildOverview();

    setWindowTitle(QString::fromStdString(overview.title));
    resize(1080, 720);

    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);

    auto* title = new QLabel(QString::fromStdString(overview.title), centralWidget);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: 600;");

    auto* engineSummary = new QLabel(QString::fromStdString(overview.engineSummary), centralWidget);
    engineSummary->setAlignment(Qt::AlignCenter);
    engineSummary->setWordWrap(true);
    engineSummary->setStyleSheet("font-size: 15px; color: #444;");

    auto* domainSummary = new QLabel(QString::fromStdString(overview.domainSummary), centralWidget);
    domainSummary->setAlignment(Qt::AlignCenter);
    domainSummary->setWordWrap(true);
    domainSummary->setStyleSheet("font-size: 15px; color: #444;");

    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(12);
    layout->addWidget(engineSummary);
    layout->addSpacing(8);
    layout->addWidget(domainSummary);
    layout->addStretch();

    setCentralWidget(centralWidget);
}
