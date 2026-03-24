#include "VideoFrameWidget.h"

#include <QPainter>

namespace {

QRect fitRectPreservingAspectRatio(const QSize& sourceSize, const QRect& bounds)
{
    if (!sourceSize.isValid() || bounds.isEmpty()) {
        return bounds;
    }

    QSize scaled = sourceSize;
    scaled.scale(bounds.size(), Qt::KeepAspectRatio);
    const int x = bounds.x() + (bounds.width() - scaled.width()) / 2;
    const int y = bounds.y() + (bounds.height() - scaled.height()) / 2;
    return QRect(QPoint(x, y), scaled);
}

} // namespace

VideoFrameWidget::VideoFrameWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoFrameWidget::setFrame(const QImage& frame)
{
    m_frame = frame;
    update();
}

void VideoFrameWidget::clearFrame(const QString& placeholderText)
{
    m_frame = QImage();
    m_placeholderText = placeholderText;
    update();
}

QSize VideoFrameWidget::sizeHint() const
{
    return QSize(640, 360);
}

void VideoFrameWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(6, 10, 16));

    if (!m_frame.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(fitRectPreservingAspectRatio(m_frame.size(), rect()), m_frame);
        return;
    }

    painter.setPen(QColor(214, 220, 232));
    painter.drawText(rect(), Qt::AlignCenter, m_placeholderText);
}
