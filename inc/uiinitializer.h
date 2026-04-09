#ifndef UIINITIALIZER_H
#define UIINITIALIZER_H

namespace Ui {
class MainWindow;
}

class uiinitializer
{
public:
    uiinitializer();

    static void setPlayPauseIcon(Ui::MainWindow *ui, bool isPlaying);
    static void init_icon(Ui::MainWindow *ui);
    static void updateVolumeIcon(Ui::MainWindow *ui, bool isMuted);
};

#endif // UIINITIALIZER_H
