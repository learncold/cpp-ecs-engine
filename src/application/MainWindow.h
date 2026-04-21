#pragma once

#include <QMainWindow>

namespace safecrowd::domain {
class SafeCrowdDomain;
}

class QWidget;

namespace safecrowd::application {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent = nullptr);
};

}  // namespace safecrowd::application
