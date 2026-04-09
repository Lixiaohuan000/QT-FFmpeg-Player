#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAudioSink>
#include <QElapsedTimer>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
}

class VideoPlayer : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    bool isPlaying() const { return m_state == Playing; }

public slots:
    void startPlay(const QString &videoPath);
    void stopPlay();
    void pause();
    void resume();
    void seek(int percent);
    void setVolume(int volume);

signals:
    void sigSendImage(QImage img);
    void sigPlaybackState(bool playing);
    void sigTimeChanged(qint64 currentMs, qint64 totalMs);
    void sigError(QString errMsg);
    void sigPlayFinished();

private slots:
    void decodeLoop(const QString &videoPath);

private:
    enum State { Stopped, Playing, Paused };
    State m_state;
    QMutex m_mutex;
    QWaitCondition m_pauseCond;
    QThread *m_workThread;

    // 音频相关（加锁保护）
    QAudioSink *m_audioSink;
    QIODevice *m_audioDevice;
    QMutex m_audioMutex;           // 保护 m_audioSink / m_audioDevice
    qint64 m_audioBytesWritten;
    double m_audioClock;
    int m_audioSampleRate;
    int m_audioChannels;
    int m_audioBytesPerSample;
    int m_volumePercent;

    // FFmpeg 相关
    AVFormatContext *m_fmtCtx;
    AVCodecContext *m_videoCtx;
    AVCodecContext *m_audioCtx;
    QMutex m_fmtCtxMutex;

    // 运行控制
    std::atomic<bool> m_isRunning;   // 原子变量
    bool m_decodeFinished;
    QWaitCondition m_stopCond;

    // Seek 请求
    bool m_seekRequest;
    int m_seekPercent;
    QMutex m_seekMutex;
};

#endif // VIDEOPLAYER_H