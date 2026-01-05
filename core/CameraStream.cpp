#include "CameraStream.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <QMetaObject>

CameraStream::CameraStream(QObject* parent)
    : QObject(parent)
{
    // Importante para RTSP
    avformat_network_init();
}

CameraStream::~CameraStream()
{
    stop();
    avformat_network_deinit();
}

void CameraStream::start(const QString& rtspUrl)
{
    stop();

    m_running = true;
    m_thread = std::thread([this, rtspUrl]() {
        run(rtspUrl);
        });
}

void CameraStream::stop()
{
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

static void emitQueued(QObject* obj, const char* signalName, const QVariant& v)
{
    // helper opcional (no usado en este archivo)
    Q_UNUSED(obj);
    Q_UNUSED(signalName);
    Q_UNUSED(v);
}

void CameraStream::run(QString url)
{
    AVFormatContext* fmt = nullptr;
    AVDictionary* opts = nullptr;

    // Recomendado para Tapo: TCP (menos problemas que UDP)
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);

    // Timeout de conexión (microsegundos). 5s:
    av_dict_set(&opts, "stimeout", "5000000", 0);

    // Menos buffer = menos latencia (puedes ajustar luego)
    av_dict_set(&opts, "fflags", "nobuffer", 0);

    // Abrir stream
    const QByteArray urlUtf8 = url.toUtf8();
    int ret = avformat_open_input(&fmt, urlUtf8.constData(), nullptr, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        QMetaObject::invokeMethod(this, [this, err]() {
            emit error(QString("avformat_open_input failed: %1").arg(err));
            }, Qt::QueuedConnection);
        return;
    }

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("avformat_find_stream_info failed");
            }, Qt::QueuedConnection);
        avformat_close_input(&fmt);
        return;
    }

    // Encontrar stream de vídeo
    int videoStreamIndex = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex < 0) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("No video stream found");
            }, Qt::QueuedConnection);
        avformat_close_input(&fmt);
        return;
    }

    AVCodecParameters* codecpar = fmt->streams[videoStreamIndex]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("Decoder not found");
            }, Qt::QueuedConnection);
        avformat_close_input(&fmt);
        return;
    }

    AVCodecContext* decCtx = avcodec_alloc_context3(decoder);
    if (!decCtx) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("avcodec_alloc_context3 failed");
            }, Qt::QueuedConnection);
        avformat_close_input(&fmt);
        return;
    }

    ret = avcodec_parameters_to_context(decCtx, codecpar);
    if (ret < 0) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("avcodec_parameters_to_context failed");
            }, Qt::QueuedConnection);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    // Abrir decoder
    ret = avcodec_open2(decCtx, decoder, nullptr);
    if (ret < 0) {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        QMetaObject::invokeMethod(this, [this, err]() {
            emit error(QString("avcodec_open2 failed: %1").arg(err));
            }, Qt::QueuedConnection);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    QMetaObject::invokeMethod(this, [this, decCtx]() {
        emit info(QString("Connected. %1x%2 pix_fmt=%3")
            .arg(decCtx->width)
            .arg(decCtx->height)
            .arg(static_cast<int>(decCtx->pix_fmt)));
        }, Qt::QueuedConnection);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* rgbFrame = av_frame_alloc();

    if (!pkt || !frame || !rgbFrame) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("Failed to allocate packet/frame");
            }, Qt::QueuedConnection);
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    // SWS: convertir al formato que entiende QImage (RGB24)
    SwsContext* sws = sws_getContext(
        decCtx->width, decCtx->height, decCtx->pix_fmt,
        decCtx->width, decCtx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error("sws_getContext failed");
            }, Qt::QueuedConnection);
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmt);
        return;
    }

    // Buffer para RGB
    int rgbBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, decCtx->width, decCtx->height, 1);
    uint8_t* rgbBuf = (uint8_t*)av_malloc(rgbBufSize);

    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbBuf,
        AV_PIX_FMT_RGB24, decCtx->width, decCtx->height, 1);

    // Loop principal
    while (m_running) {
        ret = av_read_frame(fmt, pkt);
        if (ret < 0) {
            // fin / error / timeout; puedes implementar reconexión en el siguiente paso
            break;
        }

        if (pkt->stream_index == videoStreamIndex) {
            ret = avcodec_send_packet(decCtx, pkt);
            if (ret >= 0) {
                while (ret >= 0) {
                    ret = avcodec_receive_frame(decCtx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        break;

                    // Convertir a RGB
                    sws_scale(sws,
                        frame->data, frame->linesize,
                        0, decCtx->height,
                        rgbFrame->data, rgbFrame->linesize);

                    // Crear QImage (copiamos para que sea seguro fuera del buffer)
                    QImage img(rgbFrame->data[0],
                        decCtx->width,
                        decCtx->height,
                        rgbFrame->linesize[0],
                        QImage::Format_RGB888);

                    QImage copy = img.copy();

                    QMetaObject::invokeMethod(this, [this, copy]() {
                        emit frameReady(copy);
                        }, Qt::QueuedConnection);
                }
            }
        }

        av_packet_unref(pkt);
    }

    // cleanup
    av_free(rgbBuf);
    sws_freeContext(sws);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&rgbFrame);

    avcodec_free_context(&decCtx);
    avformat_close_input(&fmt);

    QMetaObject::invokeMethod(this, [this]() {
        emit info("Stream stopped.");
        }, Qt::QueuedConnection);
}
