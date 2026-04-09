#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QStringList>

class FileManager;
class VideoPlayer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_selectFolderBtn_clicked();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onPlayPause();
    void onStop();
    void onVolumeChanged(int value);
    void onProgressSliderMoved(int value);
    void updatePlaybackState(bool playing);
    void updateTime(qint64 current, qint64 total);
    void updateVolumeIcon(int volume);
    void onPlayFinished();
    void on_nextBtn_clicked();
    void on_prevBtn_clicked();

private:
    Ui::MainWindow *ui;
    FileManager *m_fileManager;
    VideoPlayer *m_videoPlayer;
    int m_currentIndex;
    QStringList m_videoPaths;
    bool m_autoPlaying;
    bool m_isSliderPressed;
    bool m_isManualSwitch;
};

#endif // MAINWINDOW_H