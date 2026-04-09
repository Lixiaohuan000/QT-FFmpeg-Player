#include "videoplayer.h"
#include <QDebug>
#include <QMutexLocker>
#include <QMetaObject>
#include <QMediaDevices>
#include <cmath>

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent)
    , m_state(Stopped)
    , m_workThread(nullptr)
    , m_audioSink(nullptr)
    , m_audioDevice(nullptr)
    , m_audioBytesWritten(0)
    , m_audioClock(0.0)
    , m_audioSampleRate(44100)
    , m_audioChannels(2)
    , m_audioBytesPerSample(2)
    , m_volumePercent(50)
    , m_fmtCtx(nullptr)
    , m_videoCtx(nullptr)
    , m_audioCtx(nullptr)
    , m_isRunning(false)
    , m_decodeFinished(true)
    , m_seekRequest(false)
    , m_seekPercent(0)
{
    avformat_network_init();
    m_workThread = new QThread(this);
    this->moveToThread(m_workThread);
    m_workThread->start();
}

VideoPlayer::~VideoPlayer()
{
    stopPlay();
    m_workThread->quit();
    m_workThread->wait(1000);
    avformat_network_deinit();
}

void VideoPlayer::startPlay(const QString &videoPath)
{
    if (m_state != Stopped)
        stopPlay();

    QMutexLocker locker(&m_mutex);
    m_state = Playing;
    m_isRunning = true;
    m_decodeFinished = false;
    QMetaObject::invokeMethod(this, "decodeLoop", Qt::QueuedConnection, Q_ARG(QString, videoPath));
    // emit sigPlaybackState(true); // 由主界面手动控制
}

void VideoPlayer::stopPlay()
{
    {
        QMutexLocker locker(&m_mutex);
        m_state = Stopped;
        m_isRunning = false;
        m_pauseCond.wakeAll();
    }

    // 等待解码线程完全退出
    QMutexLocker locker(&m_mutex);
    while (!m_decodeFinished) {
        m_stopCond.wait(&m_mutex);
    }
}

void VideoPlayer::pause()
{
    if (m_state == Playing)
    {
        m_state = Paused;
        // emit sigPlaybackState(false);
    }
}

void VideoPlayer::resume()
{
    if (m_state == Paused)
    {
        m_state = Playing;
        m_pauseCond.wakeAll();
        // emit sigPlaybackState(true);
    }
}

void VideoPlayer::seek(int percent)
{
    QMutexLocker locker(&m_seekMutex);
    m_seekRequest = true;
    m_seekPercent = qBound(0, percent, 100);
}

void VideoPlayer::setVolume(int volume)
{
    m_volumePercent = qBound(0, volume, 100);
    QMutexLocker locker(&m_audioMutex);
    if (m_audioSink)
    {
        m_audioSink->setVolume(m_volumePercent / 100.0);
    }
}

void VideoPlayer::decodeLoop(const QString &videoPath)
{
    qDebug() << "开始解码：" << videoPath;

    // 局部变量
    AVFormatContext *fmtCtx = nullptr;
    int videoIdx = -1, audioIdx = -1;
    AVCodecContext *videoCtx = nullptr, *audioCtx = nullptr;
    SwsContext *swsCtx = nullptr;
    SwrContext *swrCtx = nullptr;
    AVFrame *frame = nullptr, *audioFrame = nullptr;
    AVPacket *pkt = nullptr;
    uint8_t *rgbBuf = nullptr;
    uint8_t *audioBuf = nullptr;
    QAudioFormat audioFormat;

    auto localCleanup = [&]()
    {
        if (rgbBuf)
            av_free(rgbBuf);
        if (audioBuf)
            av_free(audioBuf);
        if (pkt)
            av_packet_free(&pkt);
        if (frame)
            av_frame_free(&frame);
        if (audioFrame)
            av_frame_free(&audioFrame);
        if (swsCtx)
            sws_freeContext(swsCtx);
        if (swrCtx)
            swr_free(&swrCtx);
        if (videoCtx)
            avcodec_free_context(&videoCtx);
        if (audioCtx)
            avcodec_free_context(&audioCtx);
        if (fmtCtx)
            avformat_close_input(&fmtCtx);
    };

    auto handleError = [&](const QString &errMsg)
    {
        emit sigError(errMsg);
        localCleanup();
        {
            QMutexLocker locker(&m_mutex);
            m_decodeFinished = true;
            m_stopCond.wakeAll();
        }
        emit sigPlayFinished();
    };

    // 1. 打开文件
    if (avformat_open_input(&fmtCtx, videoPath.toStdString().c_str(), nullptr, nullptr) < 0)
    {
        handleError("无法打开文件");
        return;
    }
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
    {
        handleError("无法获取流信息");
        return;
    }

    // 2. 查找音视频流
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i)
    {
        auto type = fmtCtx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && videoIdx == -1) videoIdx = i;
        if (type == AVMEDIA_TYPE_AUDIO && audioIdx == -1) audioIdx = i;
    }
    if (videoIdx == -1)
    {
        handleError("未找到视频流");
        return;
    }

    // 3. 初始化视频解码器，计算帧间隔
    double frameInterval = 40.0; // 默认 25fps
    {
        auto par = fmtCtx->streams[videoIdx]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (!codec) { handleError("找不到视频解码器"); return; }
        videoCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(videoCtx, par);
        if (avcodec_open2(videoCtx, codec, nullptr) < 0)
        {
            handleError("打开视频解码器失败");
            return;
        }
        swsCtx = sws_getContext(videoCtx->width, videoCtx->height, videoCtx->pix_fmt,
                                videoCtx->width, videoCtx->height, AV_PIX_FMT_RGB24,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        frame = av_frame_alloc();
        pkt = av_packet_alloc();
        int size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoCtx->width, videoCtx->height, 1);
        rgbBuf = (uint8_t*)av_malloc(size);
        double fps = av_q2d(av_guess_frame_rate(fmtCtx, fmtCtx->streams[videoIdx], nullptr));
        if (fps > 0)
            frameInterval = 1000.0 / fps;
    }

    // 4. 初始化音频解码器和输出（只负责播放，不做同步）
    if (audioIdx >= 0)
    {
        auto par = fmtCtx->streams[audioIdx]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (codec)
        {
            audioCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(audioCtx, par);
            if (avcodec_open2(audioCtx, codec, nullptr) == 0)
            {
                audioFormat.setSampleRate(m_audioSampleRate);
                audioFormat.setChannelCount(m_audioChannels);
                audioFormat.setSampleFormat(QAudioFormat::Int16);
                if (audioFormat.isValid())
                {
                    QAudioDevice device = QMediaDevices::defaultAudioOutput();
                    if (!device.isFormatSupported(audioFormat))
                        audioFormat = device.preferredFormat();

                    // 加锁创建音频对象
                    {
                        QMutexLocker locker(&m_audioMutex);
                        m_audioSink = new QAudioSink(device, audioFormat, this);
                        m_audioSink->setVolume(m_volumePercent / 100.0);
                        m_audioDevice = m_audioSink->start();
                    }

                    // 重采样器
                    AVChannelLayout src_ch = audioCtx->ch_layout;
                    AVChannelLayout dst_ch;
                    av_channel_layout_default(&dst_ch, m_audioChannels);
                    swr_alloc_set_opts2(&swrCtx, &dst_ch, AV_SAMPLE_FMT_S16, m_audioSampleRate,
                                        &src_ch, audioCtx->sample_fmt, audioCtx->sample_rate,
                                        0, nullptr);
                    if (swrCtx)
                        swr_init(swrCtx);
                    audioFrame = av_frame_alloc();
                }
            }
        }
    }

    // 保存成员变量（供外部调用）
    {
        QMutexLocker locker(&m_fmtCtxMutex);
        m_fmtCtx = fmtCtx;
        m_videoCtx = videoCtx;
        m_audioCtx = audioCtx;
    }

    qint64 totalMs = (fmtCtx->duration / AV_TIME_BASE) * 1000;
    emit sigTimeChanged(0, totalMs);

    m_isRunning = true;
    m_decodeFinished = false;

    // 帧间隔计时器
    QElapsedTimer frameTimer;
    frameTimer.start();

    // 主解码循环
    while (m_isRunning && av_read_frame(fmtCtx, pkt) >= 0)
    {
        // 处理 seek 请求
        {
            QMutexLocker locker(&m_seekMutex);
            if (m_seekRequest)
            {
                m_seekRequest = false;
                int percent = m_seekPercent;
                int64_t duration = fmtCtx->duration;
                if (duration > 0)
                {
                    int64_t target = duration * percent / 100;
                    target = qBound(int64_t(0), target, duration);
                    int ret = av_seek_frame(fmtCtx, -1, target, AVSEEK_FLAG_BACKWARD);
                    if (ret >= 0)
                    {
                        avcodec_flush_buffers(videoCtx);
                        if (audioCtx) avcodec_flush_buffers(audioCtx);

                        // 清空音频设备缓冲区，避免旧数据残留（加锁）
                        {
                            QMutexLocker locker(&m_audioMutex);
                            if (m_audioSink)
                            {
                                m_audioSink->reset();
                                m_audioDevice = m_audioSink->start();
                            }
                        }
                        // 重置帧计时器
                        frameTimer.restart();
                        qDebug() << "seek to" << percent << "%";
                    }
                    else
                    {
                        qDebug() << "seek failed";
                    }
                }
            }
        }

        if (m_state == Paused)
        {
            QMutexLocker locker(&m_mutex);
            m_pauseCond.wait(&m_mutex);
            continue;
        }

        // 视频处理
        if (pkt->stream_index == videoIdx)
        {
            if (avcodec_send_packet(videoCtx, pkt) == 0)
            {
                while (avcodec_receive_frame(videoCtx, frame) == 0 && m_isRunning)
                {
                    double elapsed = frameTimer.elapsed();
                    if (elapsed < frameInterval)
                    {
                        int sleepMs = static_cast<int>(frameInterval - elapsed);
                        if (sleepMs > 0)
                            QThread::msleep(sleepMs);
                    }
                    frameTimer.restart();

                    double pts = 0.0;
                    int64_t pts_raw = frame->best_effort_timestamp;
                    if (pts_raw == AV_NOPTS_VALUE) pts_raw = frame->pts;
                    if (pts_raw != AV_NOPTS_VALUE)
                    {
                        AVRational tb = fmtCtx->streams[videoIdx]->time_base;
                        pts = av_q2d(tb) * pts_raw;
                    }

                    QImage img(videoCtx->width, videoCtx->height, QImage::Format_RGB888);
                    uint8_t *dstData[1] = { img.bits() };
                    int dstLinesize[1] = { static_cast<int>(img.bytesPerLine()) };
                    sws_scale(swsCtx, frame->data, frame->linesize, 0, videoCtx->height,
                              dstData, dstLinesize);
                    emit sigSendImage(img);

                    qint64 currentMs = pts * 1000;
                    emit sigTimeChanged(currentMs, totalMs);
                }
            }
        }
        // 音频处理
        else if (pkt->stream_index == audioIdx && audioCtx && swrCtx)
        {
            // 获取当前音频设备（加锁读取）
            QIODevice *dev = nullptr;
            {
                QMutexLocker locker(&m_audioMutex);
                dev = m_audioDevice;
            }
            if (!dev)
                continue;

            if (avcodec_send_packet(audioCtx, pkt) == 0)
            {
                while (avcodec_receive_frame(audioCtx, audioFrame) == 0 && m_isRunning)
                {
                    int out_samples = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
                    int bufSize = av_samples_get_buffer_size(nullptr, m_audioChannels, out_samples,
                                                             AV_SAMPLE_FMT_S16, 1);
                    if (bufSize > 0)
                    {
                        audioBuf = (uint8_t*)av_realloc(audioBuf, bufSize);
                        int samples = swr_convert(swrCtx, &audioBuf, out_samples,
                                                  (const uint8_t**)audioFrame->data, audioFrame->nb_samples);
                        int outSize = av_samples_get_buffer_size(nullptr, m_audioChannels, samples,
                                                                 AV_SAMPLE_FMT_S16, 1);
                        if (outSize > 0)
                        {
                            // 等待音频设备有空闲（加锁检查 bytesFree）
                            int waitCount = 0;
                            while (m_isRunning && waitCount < 50)
                            {
                                int free;
                                {
                                    QMutexLocker locker(&m_audioMutex);
                                    if (!m_audioSink) break;
                                    free = m_audioSink->bytesFree();
                                }
                                if (free >= outSize)
                                    break;
                                QThread::msleep(5);
                                waitCount++;
                            }
                            // 再次检查设备是否有效并写入
                            {
                                QMutexLocker locker(&m_audioMutex);
                                if (m_audioSink && m_audioDevice)
                                {
                                    m_audioDevice->write((const char*)audioBuf, outSize);
                                }
                            }
                        }
                    }
                }
            }
        }

        av_packet_unref(pkt);
    }

    // 等待音频播放完毕（加锁访问状态）
    if (m_audioSink)
    {
        int waitCount = 0;
        while (m_isRunning && waitCount < 100)
        {
            QAudio::State state;
            {
                QMutexLocker locker(&m_audioMutex);
                if (!m_audioSink)
                    break;
                state = m_audioSink->state();
            }
            if (state != QAudio::ActiveState)
                break;
            QThread::msleep(10);
            waitCount++;
        }
    }

    localCleanup();

    // 清理音频对象（加锁）
    {
        QMutexLocker locker(&m_audioMutex);
        if (m_audioSink)
        {
            delete m_audioSink;
            m_audioSink = nullptr;
            m_audioDevice = nullptr;
        }
    }

    {
        QMutexLocker locker(&m_fmtCtxMutex);
        m_fmtCtx = nullptr;
        m_videoCtx = nullptr;
        m_audioCtx = nullptr;
    }

    emit sigPlayFinished();
    {
        QMutexLocker locker(&m_mutex);
        m_decodeFinished = true;
        m_stopCond.wakeAll();
    }
}