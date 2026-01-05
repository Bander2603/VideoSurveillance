#pragma once

#include <QObject>
#include <QImage>
#include <QString>

#include <atomic>
#include <thread>

class CameraStream : public QObject
{
    Q_OBJECT
public:
    explicit CameraStream(QObject* parent = nullptr);
    ~CameraStream();

    // Inicia la captura/decodificación en un hilo interno
    void start(const QString& rtspUrl);

    // Para y libera recursos
    void stop();

signals:
    void frameReady(const QImage& frame);
    void error(const QString& message);
    void info(const QString& message);

private:
    void run(QString url);

private:
    std::atomic_bool m_running{ false };
    std::thread m_thread;
};