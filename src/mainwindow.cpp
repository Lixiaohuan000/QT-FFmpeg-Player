#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "filemanager.h"
#include "videoplayer.h"
#include <QFileDialog>
#include <QFileInfo>
#include "uiinitializer.h"
#include <QDebug>
#include <qtimer.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentIndex(-1)
    , m_autoPlaying(false)
    , m_isSliderPressed(false)
    , m_isManualSwitch(false)
{
    ui->setupUi(this);
    uiinitializer::init_icon(ui);
    ui->videoLabel->setAlignment(Qt::AlignCenter);

    m_fileManager = new FileManager(this);
    m_videoPlayer = new VideoPlayer(nullptr);

    // 连接信号槽
    connect(ui->videoListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(ui->playBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);
    connect(ui->volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    connect(ui->progressSlider, &QSlider::sliderPressed, [this](){ m_isSliderPressed = true; });
    connect(ui->progressSlider, &QSlider::sliderReleased, [this](){
        m_isSliderPressed = false;
        onProgressSliderMoved(ui->progressSlider->value());
    });

    connect(m_videoPlayer, &VideoPlayer::sigSendImage, this, [this](const QImage &img){
        ui->videoLabel->setPixmap(QPixmap::fromImage(img).scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    //connect(m_videoPlayer, &VideoPlayer::sigPlaybackState, this, &MainWindow::updatePlaybackState);
    connect(m_videoPlayer, &VideoPlayer::sigTimeChanged, this, &MainWindow::updateTime);
    connect(m_videoPlayer, &VideoPlayer::sigError, this, [](const QString &err){
        qDebug() << "Error:" << err;
    });

}

MainWindow::~MainWindow()
{
    delete m_videoPlayer;
    delete ui;
}

void MainWindow::on_selectFolderBtn_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, "select folder");
    if (path.isEmpty())
        return;

    ui->folderPathLabel->setText(path);
    QFileInfoList fileList = m_fileManager->getVideoFiles(path);
    ui->videoListWidget->clear();
    m_videoPaths.clear();
    for (const QFileInfo &info : fileList)
    {
        QListWidgetItem *item = new QListWidgetItem(info.fileName());
        QString fullPath = info.absoluteFilePath();
        item->setData(Qt::UserRole, fullPath);
        ui->videoListWidget->addItem(item);
        m_videoPaths.append(fullPath);
    }
}

void MainWindow::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    m_isManualSwitch = true;
    disconnect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);

    m_currentIndex = ui->videoListWidget->row(item);
    QString videoPath = item->data(Qt::UserRole).toString();
    m_videoPlayer->stopPlay();
    updatePlaybackState(m_videoPlayer->isPlaying());
    m_videoPlayer->startPlay(videoPath);
    ui->videoListWidget->setCurrentRow(m_currentIndex);

    onVolumeChanged(ui->volumeSlider->value());
    connect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);
    m_isManualSwitch = false;
    updatePlaybackState(m_videoPlayer->isPlaying());
}

void MainWindow::onPlayPause()
{
    if (ui->progressSlider->value() == 0)
    {
        QListWidgetItem *currentItem = ui->videoListWidget->currentItem();
        if (!currentItem)
            return;
        onItemDoubleClicked(currentItem);
            return;
    }
    if (m_videoPlayer->isPlaying())
        m_videoPlayer->pause();
    else
        m_videoPlayer->resume();
    updatePlaybackState(m_videoPlayer->isPlaying());
}

void MainWindow::onStop()
{
    m_videoPlayer->seek(0);
    // 延迟 50ms 再暂停，让 seek 有执行机会
    QTimer::singleShot(50, this, [this]() {
        m_videoPlayer->pause();
        ui->progressSlider->setValue(0);
        ui->timeLabel->setText("00:00:00 / 00:00:00");
    });
    updatePlaybackState(false);
}

void MainWindow::onVolumeChanged(int value)
{
    m_videoPlayer->setVolume(value);
    updateVolumeIcon(value);
}

void MainWindow::onProgressSliderMoved(int value)
{
    m_videoPlayer->seek(value);
}

void MainWindow::updatePlaybackState(bool playing)
{
    uiinitializer::setPlayPauseIcon(ui, playing);
}

void MainWindow::updateTime(qint64 current, qint64 total)
{
    if (!m_isSliderPressed)
    {
        int percent = (total > 0) ? (current * 100 / total) : 0;
        ui->progressSlider->setValue(percent);
    }
    qint64 curSec = current / 1000;
    int h1 = curSec / 3600;
    int m1 = (curSec % 3600) / 60;
    int s1 = curSec % 60;
    QString curStr = QString("%1:%2:%3")
                         .arg(h1, 2, 10, QChar('0'))
                         .arg(m1, 2, 10, QChar('0'))
                         .arg(s1, 2, 10, QChar('0'));

    qint64 totalSec = total / 1000;
    int h2 = totalSec / 3600;
    int m2 = (totalSec % 3600) / 60;
    int s2 = totalSec % 60;
    QString totalStr = QString("%1:%2:%3")
                           .arg(h2, 2, 10, QChar('0'))
                           .arg(m2, 2, 10, QChar('0'))
                           .arg(s2, 2, 10, QChar('0'));

    ui->timeLabel->setText(curStr + " / " + totalStr);
}

void MainWindow::updateVolumeIcon(int volume)
{
    bool isMuted = (volume == 0);
    uiinitializer::updateVolumeIcon(ui, isMuted);
    m_videoPlayer->setVolume(volume);
    ui->volumeLabel->setText(QString("%1%").arg(volume));
}

void MainWindow::onPlayFinished()
{
    if (m_isManualSwitch)
        return;
    if (m_videoPaths.isEmpty())
        return;

    updatePlaybackState(false);
    // 临时断开连接，避免递归
    disconnect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);

    m_currentIndex = (m_currentIndex + 1) % m_videoPaths.size();
    QString nextPath = m_videoPaths.at(m_currentIndex);
    m_videoPlayer->startPlay(nextPath);
    updatePlaybackState(m_videoPlayer->isPlaying());
    ui->videoListWidget->setCurrentRow(m_currentIndex);
    // 重新连接
    connect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);
}

void MainWindow::on_nextBtn_clicked()
{
    if (m_videoPaths.isEmpty()) return;

    m_isManualSwitch = true;
    disconnect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);

    m_currentIndex = (m_currentIndex + 1) % m_videoPaths.size();
    QString nextPath = m_videoPaths.at(m_currentIndex);
    m_videoPlayer->stopPlay();
    m_videoPlayer->startPlay(nextPath);
    ui->videoListWidget->setCurrentRow(m_currentIndex);

    connect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);
    m_isManualSwitch = false;
    updatePlaybackState(m_videoPlayer->isPlaying());
}

void MainWindow::on_prevBtn_clicked()
{
    if (m_videoPaths.isEmpty())
        return;


    m_isManualSwitch = true;
    disconnect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);

    m_currentIndex = (m_currentIndex - 1 + m_videoPaths.size()) % m_videoPaths.size();
    QString prevPath = m_videoPaths.at(m_currentIndex);
    m_videoPlayer->stopPlay();
    m_videoPlayer->startPlay(prevPath);
    ui->videoListWidget->setCurrentRow(m_currentIndex);

    connect(m_videoPlayer, &VideoPlayer::sigPlayFinished, this, &MainWindow::onPlayFinished);
    m_isManualSwitch = false;
    updatePlaybackState(m_videoPlayer->isPlaying());
}

