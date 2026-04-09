#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QFileInfoList>

class FileManager : public QObject
{
    Q_OBJECT
public:
    explicit FileManager(QObject *parent = nullptr);
    QFileInfoList getVideoFiles(const QString &dirPath);
};

#endif // FILEMANAGER_H