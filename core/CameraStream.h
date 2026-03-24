#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QSize>

#include <atomic>
#include <cstdint>
#include <thread>

class CameraStream : public QObject
{
    Q_OBJECT
public:
    explicit CameraStream(QObject* parent = nullptr);
    ~CameraStream();

    void start(const QString& rtspUrl);
    void stop();

signals:
    void frameReady(const QImage& frame);
    void streamGeometryChanged(const QSize& frameSize);
    void statusChanged(const QString& state, const QString& detail);
    void error(const QString& message);
    void info(const QString& message);

private:
    void workerLoop(QString url);
    void run(QString url);
    void setBlockingDeadlineMs(std::int64_t deadlineMs);

private:
    std::atomic_bool m_running{ false };
    std::atomic_bool m_abortBlocking{ false };
    std::atomic_bool m_connectedSinceLastAttempt{ false };
    std::thread m_thread;
    std::atomic<std::int64_t> m_blockingDeadlineMs{ 0 };
};
