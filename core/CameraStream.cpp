#include "CameraStream.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

namespace {

constexpr int kOpenTimeoutMs = 5000;
constexpr int kReadTimeoutMs = 10000;
constexpr int kReconnectInitialDelayMs = 1000;
constexpr int kReconnectMaxDelayMs = 15000;

struct InterruptContext
{
    std::atomic_bool* abortBlocking = nullptr;
    std::atomic<std::int64_t>* blockingDeadlineMs = nullptr;
};

std::int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int ffmpegInterruptCallback(void* opaque)
{
    auto* ctx = static_cast<InterruptContext*>(opaque);
    if (!ctx || !ctx->abortBlocking || !ctx->blockingDeadlineMs) {
        return 0;
    }

    if (ctx->abortBlocking->load()) {
        return 1;
    }

    const std::int64_t deadlineMs = ctx->blockingDeadlineMs->load();
    return deadlineMs > 0 && nowMs() > deadlineMs ? 1 : 0;
}

void ensureNetworkInit()
{
    static std::once_flag once;
    std::call_once(once, []() {
        avformat_network_init();
    });
}

QString ffmpegErrorString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, errorBuffer, sizeof(errorBuffer));
    return QString::fromUtf8(errorBuffer);
}

void invokeStatus(CameraStream* stream, const QString& state, const QString& detail)
{
    QMetaObject::invokeMethod(stream, [stream, state, detail]() {
        emit stream->statusChanged(state, detail);
    }, Qt::QueuedConnection);
}

void invokeError(CameraStream* stream, const QString& message)
{
    QMetaObject::invokeMethod(stream, [stream, message]() {
        emit stream->error(message);
    }, Qt::QueuedConnection);
}

void invokeInfo(CameraStream* stream, const QString& message)
{
    QMetaObject::invokeMethod(stream, [stream, message]() {
        emit stream->info(message);
    }, Qt::QueuedConnection);
}

} // namespace

CameraStream::CameraStream(QObject* parent)
    : QObject(parent)
{
    ensureNetworkInit();
}

CameraStream::~CameraStream()
{
    stop();
}

void CameraStream::start(const QString& rtspUrl)
{
    stop();

    if (rtspUrl.trimmed().isEmpty()) {
        invokeStatus(this, "offline", "Missing RTSP URL");
        return;
    }

    m_running = true;
    m_abortBlocking = false;
    m_connectedSinceLastAttempt = false;
    m_thread = std::thread([this, rtspUrl]() {
        workerLoop(rtspUrl);
    });
}

void CameraStream::stop()
{
    m_running = false;
    m_abortBlocking = true;
    setBlockingDeadlineMs(0);

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void CameraStream::workerLoop(QString url)
{
    bool firstAttempt = true;
    int reconnectDelayMs = kReconnectInitialDelayMs;

    while (m_running) {
        const QString attemptState = firstAttempt ? "connecting" : "reconnecting";
        const QString attemptDetail = firstAttempt
            ? QStringLiteral("Connecting...")
            : QStringLiteral("Reconnecting...");
        invokeStatus(this, attemptState, attemptDetail);

        m_abortBlocking = false;
        m_connectedSinceLastAttempt = false;
        run(url);

        if (!m_running) {
            break;
        }

        const bool hadConnection = m_connectedSinceLastAttempt.exchange(false);
        reconnectDelayMs = hadConnection ? kReconnectInitialDelayMs
                                         : std::min(reconnectDelayMs * 2, kReconnectMaxDelayMs);

        const int retryInSeconds = std::max(1, reconnectDelayMs / 1000);
        invokeStatus(this, "offline", QString("Retrying in %1s").arg(retryInSeconds));

        for (int waitedMs = 0; m_running && waitedMs < reconnectDelayMs; waitedMs += 200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        firstAttempt = false;
    }

    invokeStatus(this, "stopped", "Stream stopped");
    invokeInfo(this, "Stream stopped.");
}

void CameraStream::run(QString url)
{
    AVFormatContext* fmt = avformat_alloc_context();
    if (!fmt) {
        invokeError(this, "avformat_alloc_context failed");
        return;
    }

    InterruptContext interruptContext{
        .abortBlocking = &m_abortBlocking,
        .blockingDeadlineMs = &m_blockingDeadlineMs
    };
    fmt->interrupt_callback.callback = ffmpegInterruptCallback;
    fmt->interrupt_callback.opaque = &interruptContext;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "stimeout", "5000000", 0);
    av_dict_set(&opts, "rw_timeout", "10000000", 0);
    av_dict_set(&opts, "fflags", "nobuffer", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);

    setBlockingDeadlineMs(nowMs() + kOpenTimeoutMs);
    const QByteArray urlUtf8 = url.toUtf8();
    const int openResult = avformat_open_input(&fmt, urlUtf8.constData(), nullptr, &opts);
    av_dict_free(&opts);
    setBlockingDeadlineMs(0);

    if (openResult < 0) {
        const QString message = m_abortBlocking
            ? QStringLiteral("Connection cancelled")
            : QString("avformat_open_input failed: %1").arg(ffmpegErrorString(openResult));
        invokeError(this, message);
        avformat_free_context(fmt);
        return;
    }

    m_connectedSinceLastAttempt = true;

    setBlockingDeadlineMs(nowMs() + kOpenTimeoutMs);
    const int streamInfoResult = avformat_find_stream_info(fmt, nullptr);
    setBlockingDeadlineMs(0);
    if (streamInfoResult < 0) {
        invokeError(this, QString("avformat_find_stream_info failed: %1").arg(ffmpegErrorString(streamInfoResult)));
        avformat_close_input(&fmt);
        return;
    }

    int videoStreamIndex = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex < 0) {
        invokeError(this, "No video stream found");
        avformat_close_input(&fmt);
        return;
    }

    AVCodecParameters* codecpar = fmt->streams[videoStreamIndex]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        invokeError(this, "Decoder not found");
        avformat_close_input(&fmt);
        return;
    }

    AVCodecContext* decCtx = avcodec_alloc_context3(decoder);
    if (!decCtx) {
        invokeError(this, "avcodec_alloc_context3 failed");
        avformat_close_input(&fmt);
        return;
    }

    const int codecContextResult = avcodec_parameters_to_context(decCtx, codecpar);
    if (codecContextResult < 0) {
        invokeError(this, QString("avcodec_parameters_to_context failed: %1").arg(ffmpegErrorString(codecContextResult)));
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    const int openDecoderResult = avcodec_open2(decCtx, decoder, nullptr);
    if (openDecoderResult < 0) {
        invokeError(this, QString("avcodec_open2 failed: %1").arg(ffmpegErrorString(openDecoderResult)));
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    QMetaObject::invokeMethod(this, [this, decCtx]() {
        emit streamGeometryChanged(QSize(decCtx->width, decCtx->height));
        emit statusChanged("streaming", QString("%1x%2").arg(decCtx->width).arg(decCtx->height));
        emit info(QString("Connected. %1x%2 pix_fmt=%3")
            .arg(decCtx->width)
            .arg(decCtx->height)
            .arg(static_cast<int>(decCtx->pix_fmt)));
    }, Qt::QueuedConnection);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* rgbFrame = av_frame_alloc();

    if (!pkt || !frame || !rgbFrame) {
        invokeError(this, "Failed to allocate packet/frame");
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    SwsContext* sws = sws_getContext(
        decCtx->width, decCtx->height, decCtx->pix_fmt,
        decCtx->width, decCtx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws) {
        invokeError(this, "sws_getContext failed");
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    const int rgbBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, decCtx->width, decCtx->height, 1);
    uint8_t* rgbBuf = static_cast<uint8_t*>(av_malloc(rgbBufSize));
    if (!rgbBuf) {
        invokeError(this, "Failed to allocate RGB buffer");
        sws_freeContext(sws);
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    av_image_fill_arrays(
        rgbFrame->data,
        rgbFrame->linesize,
        rgbBuf,
        AV_PIX_FMT_RGB24,
        decCtx->width,
        decCtx->height,
        1);

    bool readErrored = false;

    while (m_running) {
        m_abortBlocking = false;
        setBlockingDeadlineMs(nowMs() + kReadTimeoutMs);
        const int readResult = av_read_frame(fmt, pkt);
        setBlockingDeadlineMs(0);

        if (readResult < 0) {
            if (m_running && !m_abortBlocking) {
                invokeError(this, QString("av_read_frame failed: %1").arg(ffmpegErrorString(readResult)));
                readErrored = true;
            }
            break;
        }

        if (pkt->stream_index == videoStreamIndex) {
            int sendResult = avcodec_send_packet(decCtx, pkt);
            if (sendResult < 0) {
                invokeError(this, QString("avcodec_send_packet failed: %1").arg(ffmpegErrorString(sendResult)));
                readErrored = true;
                av_packet_unref(pkt);
                break;
            }

            while (m_running) {
                const int receiveResult = avcodec_receive_frame(decCtx, frame);
                if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                    break;
                }

                if (receiveResult < 0) {
                    invokeError(this, QString("avcodec_receive_frame failed: %1").arg(ffmpegErrorString(receiveResult)));
                    readErrored = true;
                    break;
                }

                sws_scale(
                    sws,
                    frame->data,
                    frame->linesize,
                    0,
                    decCtx->height,
                    rgbFrame->data,
                    rgbFrame->linesize);

                QImage img(
                    rgbFrame->data[0],
                    decCtx->width,
                    decCtx->height,
                    rgbFrame->linesize[0],
                    QImage::Format_RGB888);

                const QImage copy = img.copy();
                QMetaObject::invokeMethod(this, [this, copy]() {
                    emit frameReady(copy);
                }, Qt::QueuedConnection);
            }

            if (readErrored) {
                av_packet_unref(pkt);
                break;
            }
        }

        av_packet_unref(pkt);
    }

    av_free(rgbBuf);
    sws_freeContext(sws);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&rgbFrame);
    avcodec_free_context(&decCtx);
    avformat_close_input(&fmt);
}

void CameraStream::setBlockingDeadlineMs(std::int64_t deadlineMs)
{
    m_blockingDeadlineMs = deadlineMs;
}
