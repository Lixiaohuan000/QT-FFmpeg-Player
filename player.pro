QT       += core gui widgets multimedia multimediawidgets

CONFIG += c++17
DEFINES += QT_DEPRECATED_WARNINGS

# 头文件路径
INCLUDEPATH += $$PWD/inc
INCLUDEPATH += $$PWD/src

# 源文件
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/videoplayer.cpp \
    src/filemanager.cpp \
    src/uiinitializer.cpp

# 头文件
HEADERS += \
    inc/mainwindow.h \
    inc/videoplayer.h \
    inc/filemanager.h \
    inc/uiinitializer.h

# UI 文件（路径正确，根目录）
FORMS += mainwindow.ui

# FFmpeg 配置
FFMPEG_DIR = D:/Downloads/ffmpeg-7.1.1-full_build-shared/ffmpeg-7.1.1-full_build-shared
INCLUDEPATH += $$FFMPEG_DIR/include
LIBS += -L$$FFMPEG_DIR/lib -lavformat -lavcodec -lavutil -lswscale -lswresample

# 资源文件
RESOURCES += images.qrc