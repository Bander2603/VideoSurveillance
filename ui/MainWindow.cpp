#include "MainWindow.h"

#include <QAction>
#include <QGridLayout>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

#include "core/CameraConfig.h"
#include "core/DotEnv.h"
#include "ui/CameraPanel.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    resize(1280, 840);
    setWindowTitle("RTSP Video Surveillance");
    setStyleSheet(
        "QMainWindow { background: #020617; }"
        "QToolBar { background: #0f172a; border: none; spacing: 8px; padding: 8px; }"
        "QToolButton { color: #e2e8f0; background: #1e293b; border: 1px solid #334155; border-radius: 6px; padding: 6px 12px; }"
        "QToolButton:hover { background: #334155; }"
        "QStatusBar { color: #94a3b8; }"
        "QWidget#cameraPanel { background: #0f172a; border: 1px solid #1e293b; border-radius: 12px; }");

    auto* toolBar = addToolBar("Controls");
    toolBar->setMovable(false);
    QAction* reloadAction = toolBar->addAction("Reload cameras");
    connect(reloadAction, &QAction::triggered, this, &MainWindow::reloadCameraGrid);

    m_central = new QWidget(this);
    setCentralWidget(m_central);

    auto* rootLayout = new QVBoxLayout(m_central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    m_grid = new QGridLayout();
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(10);
    m_grid->setVerticalSpacing(10);

    rootLayout->addLayout(m_grid, 1);

    reloadCameraGrid();
}

MainWindow::~MainWindow() = default;

void MainWindow::reloadCameraGrid()
{
    DotEnv::loadFile(".env");
    const QList<CameraConfig> configs = loadCameraConfigs();

    clearGrid();

    if (configs.isEmpty()) {
        auto* emptyStateLabel = new QLabel(
            "No cameras configured.\n"
            "Use VS_CAMERA_1_NAME / VS_CAMERA_1_URL in .env or keep the legacy VS_RTSP_* variables.",
            this);
        emptyStateLabel->setAlignment(Qt::AlignCenter);
        emptyStateLabel->setStyleSheet("color: #cbd5e1; font-size: 16px;");
        m_grid->addWidget(emptyStateLabel, 0, 0);
        statusBar()->showMessage("No cameras configured");
        return;
    }

    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(configs.size()))));
    for (int i = 0; i < configs.size(); ++i) {
        const int row = i / columns;
        const int column = i % columns;
        m_grid->addWidget(new CameraPanel(configs[i], this), row, column);
    }

    for (int column = 0; column < columns; ++column) {
        m_grid->setColumnStretch(column, 1);
    }

    const int rows = static_cast<int>(std::ceil(static_cast<double>(configs.size()) / columns));
    for (int row = 0; row < rows; ++row) {
        m_grid->setRowStretch(row, 1);
    }

    statusBar()->showMessage(QString("Loaded %1 camera(s)").arg(configs.size()));
}

void MainWindow::clearGrid()
{
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
}
