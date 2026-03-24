#pragma once

#include <QMainWindow>

class QGridLayout;
class QLabel;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void reloadCameraGrid();
    void clearGrid();

private:
    QWidget* m_central = nullptr;
    QGridLayout* m_grid = nullptr;
};
