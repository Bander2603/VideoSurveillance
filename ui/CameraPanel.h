#pragma once

#include <QSize>
#include <QWidget>

#include "core/CameraConfig.h"

class CameraStream;
class QLabel;
class VideoFrameWidget;

class CameraPanel : public QWidget
{
public:
    explicit CameraPanel(const CameraConfig& config, QWidget* parent = nullptr);
    ~CameraPanel() override;

    const CameraConfig& config() const;

private:
    void applyStatus(const QString& state, const QString& detail);
    void updateFooter();

private:
    CameraConfig m_config;
    QSize m_streamSize;
    QString m_state = "idle";
    QString m_statusDetail = "Waiting to start";

    QLabel* m_titleLabel = nullptr;
    QLabel* m_statusBadge = nullptr;
    QLabel* m_footerLabel = nullptr;
    VideoFrameWidget* m_videoWidget = nullptr;
    CameraStream* m_stream = nullptr;
};
