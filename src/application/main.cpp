#include <QApplication>

#include "application/MainWindow.h"
#include "application/UiStyle.h"
#include "domain/SafeCrowdDomain.h"
#include "engine/EngineRuntime.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(safecrowd::application::ui::appStyleSheet());

    safecrowd::engine::EngineRuntime runtime;
    safecrowd::domain::SafeCrowdDomain domain(runtime);
    safecrowd::application::MainWindow window(domain);
    window.show();

    return app.exec();
}
