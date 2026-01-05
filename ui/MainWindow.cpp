#include "MainWindow.h"

#include <QLabel>
#include <QGridLayout>
#include <QDebug>
#include <QPixmap>

#include "core/CameraStream.h"

QLabel* MainWindow::makeVideoLabel()
{
    auto* l = new QLabel();
    l->setAlignment(Qt::AlignCenter);
    l->setMinimumSize(320, 240);
    l->setStyleSheet("background-color: black; color: white;");
    l->setText("No signal");
    l->setScaledContents(false);
    return l;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    resize(1200, 800);
    setWindowTitle("RTSP Video Surveillance - Grid");

    m_central = new QWidget(this);
    setCentralWidget(m_central);

    auto* grid = new QGridLayout(m_central);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setSpacing(6);

    m_label1 = makeVideoLabel();
    m_label2 = makeVideoLabel();
    m_label3 = makeVideoLabel();
    auto* placeholder = makeVideoLabel();
    placeholder->setText("Reserved");

    grid->addWidget(m_label1, 0, 0);
    grid->addWidget(m_label2, 0, 1);
    grid->addWidget(m_label3, 1, 0);
    grid->addWidget(placeholder, 1, 1);

    // Streams (1 hilo por cámara)
    m_cam1 = new CameraStream(this);
    m_cam2 = new CameraStream(this);
    m_cam3 = new CameraStream(this);

    auto connectStreamToLabel = [](CameraStream* stream, QLabel* label, const char* tag) {
        QObject::connect(stream, &CameraStream::frameReady, label, [label](const QImage& img) {
            // Escala al tamaño actual del label
            label->setPixmap(QPixmap::fromImage(img).scaled(
                label->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
            });

        QObject::connect(stream, &CameraStream::error, label, [tag](const QString& msg) {
            qDebug() << tag << "[ERROR]" << msg;
            });

        QObject::connect(stream, &CameraStream::info, label, [tag](const QString& msg) {
            qDebug() << tag << "[INFO]" << msg;
            });
        };

    connectStreamToLabel(m_cam1, m_label1, "[TC72]");
    connectStreamToLabel(m_cam2, m_label2, "[C210]");
    connectStreamToLabel(m_cam3, m_label3, "[TC70]");

    auto must = [](const char* key) {
        const QString v = qEnvironmentVariable(key);
        if (v.isEmpty())
            qDebug() << "[CONFIG] Missing:" << key;
        return v;
        };

    const QString url1 = must("VS_RTSP_TC72");
    const QString url2 = must("VS_RTSP_C210");
    const QString url3 = must("VS_RTSP_TC70");

    if (!url1.isEmpty()) m_cam1->start(url1);
    if (!url2.isEmpty()) m_cam2->start(url2);
    if (!url3.isEmpty()) m_cam3->start(url3);
}

MainWindow::~MainWindow()
{
    if (m_cam1) m_cam1->stop();
    if (m_cam2) m_cam2->stop();
    if (m_cam3) m_cam3->stop();
}
