#include "filemanager.h"
#include <QDir>

FileManager::FileManager(QObject *parent) : QObject(parent) {}

QFileInfoList FileManager::getVideoFiles(const QString &dirPath)
{
    QDir dir(dirPath);
    QStringList filters;
    filters << "*.mp4" << "*.mkv" << "*.mov";
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
    return list;
}