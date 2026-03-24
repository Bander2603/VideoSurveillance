#include "CameraPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include "core/CameraStream.h"
#include "ui/VideoFrameWidget.h"

namespace {

QString badgeStyle(const QString& backgroundColor, const QString& textColor = "#f8fafc")
{
    return QString(
        "QLabel {"
        " background: %1;"
        " color: %2;"
        " border-radius: 10px;"
        " padding: 3px 10px;"
        " font-weight: 600;"
        " }")
        .arg(backgroundColor, textColor);
}

QString titleCaseStatus(const QString& state)
{
    if (state.isEmpty()) {
        return "Unknown";
    }

    QString text = state;
    text[0] = text[0].toUpper();
    return text;
}

} // namespace

CameraPanel::CameraPanel(const CameraConfig& config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
{
    setObjectName("cameraPanel");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_titleLabel = new QLabel(m_config.name, this);
    m_titleLabel->setStyleSheet("color: #e2e8f0; font-size: 16px; font-weight: 600;");

    m_statusBadge = new QLabel(this);
    m_statusBadge->setAlignment(Qt::AlignCenter);

    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(m_statusBadge, 0);

    m_videoWidget = new VideoFrameWidget(this);
    m_videoWidget->clearFrame("Connecting...");

    m_footerLabel = new QLabel(this);
    m_footerLabel->setStyleSheet("color: #94a3b8;");

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(m_videoWidget, 1);
    rootLayout->addWidget(m_footerLabel);

    m_stream = new CameraStream(this);

    connect(m_stream, &CameraStream::frameReady, this, [this](const QImage& frame) {
        m_videoWidget->setFrame(frame);
    });

    connect(m_stream, &CameraStream::streamGeometryChanged, this, [this](const QSize& size) {
        m_streamSize = size;
        updateFooter();
    });

    connect(m_stream, &CameraStream::statusChanged, this, [this](const QString& state, const QString& detail) {
        applyStatus(state, detail);
    });

    connect(m_stream, &CameraStream::error, this, [this](const QString& message) {
        m_statusDetail = message;
        updateFooter();
    });

    connect(m_stream, &CameraStream::info, this, [this](const QString& message) {
        m_statusDetail = message;
        updateFooter();
    });

    applyStatus("connecting", "Connecting...");
    m_stream->start(m_config.rtspUrl);
}

CameraPanel::~CameraPanel()
{
    if (m_stream) {
        m_stream->stop();
    }
}

const CameraConfig& CameraPanel::config() const
{
    return m_config;
}

void CameraPanel::applyStatus(const QString& state, const QString& detail)
{
    m_state = state;
    if (!detail.trimmed().isEmpty()) {
        m_statusDetail = detail;
    }

    if (state == "streaming") {
        m_statusBadge->setText("Live");
        m_statusBadge->setStyleSheet(badgeStyle("#15803d"));
    } else if (state == "connecting") {
        m_statusBadge->setText("Connecting");
        m_statusBadge->setStyleSheet(badgeStyle("#b45309"));
        m_videoWidget->clearFrame("Connecting...");
    } else if (state == "reconnecting") {
        m_statusBadge->setText("Reconnecting");
        m_statusBadge->setStyleSheet(badgeStyle("#c2410c"));
        m_videoWidget->clearFrame("Reconnecting...");
    } else if (state == "offline") {
        m_statusBadge->setText("Offline");
        m_statusBadge->setStyleSheet(badgeStyle("#b91c1c"));
        m_videoWidget->clearFrame("Offline");
    } else {
        m_statusBadge->setText(titleCaseStatus(state));
        m_statusBadge->setStyleSheet(badgeStyle("#475569"));
        m_videoWidget->clearFrame("Stopped");
    }

    updateFooter();
}

void CameraPanel::updateFooter()
{
    QStringList parts;
    parts.push_back(titleCaseStatus(m_state));
    if (m_streamSize.isValid()) {
        parts.push_back(QString("%1x%2").arg(m_streamSize.width()).arg(m_streamSize.height()));
    }
    if (!m_statusDetail.trimmed().isEmpty()) {
        parts.push_back(m_statusDetail);
    }

    m_footerLabel->setText(parts.join("  |  "));
}
