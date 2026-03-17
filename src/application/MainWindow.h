#pragma once

#include <QMainWindow>

namespace safecrowd
{
class SafeCrowdDomain;
}

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
};
