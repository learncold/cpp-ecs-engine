#pragma once

#include <QMainWindow>

namespace safecrowd::domain {
class SafeCrowdDomain;
}

class QLabel;
class QPushButton;
class QTimer;

namespace safecrowd::application {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent = nullptr);

private:
    void startSimulation();
    void pauseSimulation();
    void stopSimulation();
    void tickSimulation();
    void refreshRuntimePanel();

    safecrowd::domain::SafeCrowdDomain& domain_;
    QPushButton* startButton_{nullptr};
    QPushButton* pauseButton_{nullptr};
    QPushButton* stopButton_{nullptr};
    QLabel* workspaceStageValue_{nullptr};
    QLabel* runtimeStateValue_{nullptr};
    QLabel* frameValue_{nullptr};
    QLabel* fixedStepValue_{nullptr};
    QLabel* alphaValue_{nullptr};
    QLabel* runValue_{nullptr};
    QLabel* variationValue_{nullptr};
    QTimer* tickTimer_{nullptr};
};

}  // namespace safecrowd::application
