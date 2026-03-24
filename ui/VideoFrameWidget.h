#pragma once

#include <QImage>
#include <QWidget>

class VideoFrameWidget : public QWidget
{
public:
    explicit VideoFrameWidget(QWidget* parent = nullptr);

    void setFrame(const QImage& frame);
    void clearFrame(const QString& placeholderText);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_frame;
    QString m_placeholderText = "No signal";
};
