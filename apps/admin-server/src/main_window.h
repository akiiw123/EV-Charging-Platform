#pragma once

#include <QMainWindow>

namespace charging::admin {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace charging::admin
