#pragma once
#include <QMainWindow>

class QLabel;
class CameraStream;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    QLabel* makeVideoLabel();

private:
    QWidget* m_central = nullptr;

    QLabel* m_label1 = nullptr;
    QLabel* m_label2 = nullptr;
    QLabel* m_label3 = nullptr;

    CameraStream* m_cam1 = nullptr;
    CameraStream* m_cam2 = nullptr;
    CameraStream* m_cam3 = nullptr;
};