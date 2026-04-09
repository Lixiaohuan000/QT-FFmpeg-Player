# Qt FFmpeg 视频播放器
基于 Qt 和 FFmpeg 开发的跨平台桌面视频播放器，支持常见音视频格式解码播放。
![主界面](./demo/screenshot/main_interface.png)

## 功能特性
- 支持常见视频文件播放（MP4、MOV、MKV 等）
- 视频画面渲染 + 音频播放
- 播放、暂停、停止、进度调节
- 音量控制
- 列表循环，结束自动切换下一曲 
- 简洁 Qt 界面

## 开发环境
- Qt 6.11.0 (MinGW 64-bit 编译套件)
- C++17
- FFmpeg 7.1.1

## 项目结构

player/
├── inc/ # 头文件 .h
├── src/ # 源文件 .cpp
├── images/ # 图标与资源图片
├── mainwindow.ui # UI 界面文件
├── images.qrc # Qt 资源文件
├── player.pro # Qt 项目构建文件
├── README.md
└── LICENSE

## 编译与运行
1.  安装 Qt 环境（MinGW 64-bit 编译套件）
2.  下载并配置 FFmpeg 7.1.1 开发库
3.  使用 Qt Creator 打开 `player.pro`
4.  执行 qmake → 构建 → 运行

## 依赖说明
本项目依赖 FFmpeg 库，不包含二进制文件，需自行配置库路径。
- libavformat
- libavcodec
- libavutil
- libswscale
- libswresample

## 许可证
MIT License，可自由使用、修改、分发。

## 作者
Lixiaohuan000