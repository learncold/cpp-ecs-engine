#include "application/MainWindow.h"

#include <QWidget>

#include "domain/SafeCrowdDomain.h"

namespace safecrowd::application {

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent) {
    (void)domain;

    setCentralWidget(new QWidget(this));
    setWindowTitle("SafeCrowd");
    resize(1200, 800);
}

}  // namespace safecrowd::application
