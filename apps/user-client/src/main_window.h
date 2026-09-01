#pragma once

#include <QMainWindow>

namespace charging::user {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace charging::user
