#pragma once

#include <QMainWindow>

namespace safecrowd::domain {
class SafeCrowdDomain;
}

class QLabel;
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
    void refreshStatusLabel();

    safecrowd::domain::SafeCrowdDomain& domain_;
    QLabel* statusLabel_{nullptr};
    QTimer* tickTimer_{nullptr};
};

}  // namespace safecrowd::application
