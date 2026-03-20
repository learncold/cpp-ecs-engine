#include "application/MainWindow.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "domain/SafeCrowdDomain.h"
#include "engine/EngineState.h"

namespace {

QString stateToString(safecrowd::engine::EngineState state) {
    using safecrowd::engine::EngineState;

    switch (state) {
    case EngineState::Stopped:
        return "Stopped";
    case EngineState::Ready:
        return "Ready";
    case EngineState::Running:
        return "Running";
    case EngineState::Paused:
        return "Paused";
    }

    return "Unknown";
}

}  // namespace

namespace safecrowd::application {

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent),
      domain_(domain) {
    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);
    statusLabel_ = new QLabel(this);

    auto* startButton = new QPushButton("Start", this);
    auto* pauseButton = new QPushButton("Pause", this);
    auto* stopButton = new QPushButton("Stop", this);

    layout->addWidget(statusLabel_);
    layout->addWidget(startButton);
    layout->addWidget(pauseButton);
    layout->addWidget(stopButton);

    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(16);

    connect(startButton, &QPushButton::clicked, this, [this]() { startSimulation(); });
    connect(pauseButton, &QPushButton::clicked, this, [this]() { pauseSimulation(); });
    connect(stopButton, &QPushButton::clicked, this, [this]() { stopSimulation(); });
    connect(tickTimer_, &QTimer::timeout, this, [this]() { tickSimulation(); });

    setCentralWidget(centralWidget);
    setWindowTitle("SafeCrowd");
    resize(420, 220);

    refreshStatusLabel();
}

void MainWindow::startSimulation() {
    domain_.start();
    tickTimer_->start();
    refreshStatusLabel();
}

void MainWindow::pauseSimulation() {
    domain_.pause();
    tickTimer_->stop();
    refreshStatusLabel();
}

void MainWindow::stopSimulation() {
    domain_.stop();
    tickTimer_->stop();
    refreshStatusLabel();
}

void MainWindow::tickSimulation() {
    domain_.update(1.0 / 60.0);
    refreshStatusLabel();
}

void MainWindow::refreshStatusLabel() {
    const auto summary = domain_.summary();
    statusLabel_->setText(
        QString("State: %1\nFrames: %2\nFixed Steps: %3\nAlpha: %4")
            .arg(stateToString(summary.state))
            .arg(summary.frameIndex)
            .arg(summary.fixedStepIndex)
            .arg(summary.alpha, 0, 'f', 2));
}

}  // namespace safecrowd::application
