#include "uiinitializer.h"
#include "ui_mainwindow.h"

#include <QPixmap>
uiinitializer::uiinitializer() {}

static void setBtnIcon(QPushButton *btn, const QString &iconResPath)
{
    if (!btn) return; // 空指针保护
    QPixmap iconPixmap(iconResPath);

    const QSize fixedIconSize(24, 24);

    if (!iconPixmap.isNull())
    {
        btn->setIcon(QIcon(iconPixmap));
        btn->setIconSize(fixedIconSize);
        btn->setText("");
        btn->setFlat(true);
        btn->setStyleSheet("QPushButton { padding: 4px; outline: none; }");
    }
    else
    {
        btn->setIcon(QIcon());
    }
}

void uiinitializer::init_icon(Ui::MainWindow *ui)
{
    setBtnIcon(ui->selectFolderBtn, ":/images/folder.png");
    setBtnIcon(ui->stopBtn,         ":/images/stop.png");
    setBtnIcon(ui->prevBtn,         ":/images/prev.png");
    setBtnIcon(ui->nextBtn,         ":/images/next.png");
    updateVolumeIcon(ui, true);
    setPlayPauseIcon(ui, false);
}

void uiinitializer::updateVolumeIcon(Ui::MainWindow *ui, bool isMuted)
{
    if (!ui || !ui->volumeBtn)
        return;

    const QSize fixedIconSize(24, 24);
    QString iconPath = isMuted ? ":/images/mute.png" : ":/images/unmute.png";

    QPixmap iconPixmap(iconPath);
    if (!iconPixmap.isNull())
    {
        ui->volumeBtn->setIcon(QIcon(iconPixmap));
        ui->volumeBtn->setIconSize(fixedIconSize);
        ui->volumeBtn->setText("");
        ui->volumeBtn->setFlat(true);
        ui->volumeBtn->setStyleSheet("QPushButton { padding: 4px; outline: none; }");
    }
}

void uiinitializer::setPlayPauseIcon(Ui::MainWindow *ui, bool isPlaying)
{
    if (!ui || !ui->playBtn)
        return;

    const QSize fixedIconSize(24, 24);
    QString iconPath = isPlaying ? ":/images/play.png" : ":/images/pause.png";

    QPixmap pixmap(iconPath);
    if (!pixmap.isNull())
    {
        ui->playBtn->setIcon(QIcon(pixmap));
        ui->playBtn->setIconSize(fixedIconSize);
        ui->playBtn->setText("");
        ui->playBtn->setFlat(true);
        ui->playBtn->setStyleSheet("QPushButton { padding: 4px; outline: none; }");
    }
}