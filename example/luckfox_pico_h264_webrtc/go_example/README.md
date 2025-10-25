# Go H.264 接收器测试程序

## 简介

这是一个简单的 Go 测试程序，用于接收 `luckfox_pico_h264_webrtc` 发送的 H.264 视频流。

## 编译

### 在开发机上编译（交叉编译）

```bash
# ARM32 (Luckfox Pico)
GOOS=linux GOARCH=arm GOARM=7 go build -o h264_receiver h264_receiver.go

# ARM64
GOOS=linux GOARCH=arm64 go build -o h264_receiver h264_receiver.go

# x86_64 (开发机测试)
go build -o h264_receiver h264_receiver.go
```

### 在 Luckfox Pico 上编译

```bash
go build -o h264_receiver h264_receiver.go
```

## 使用

### 方法一：先启动 Go 程序

```bash
# 终端 1
./h264_receiver

# 终端 2
./luckfox_pico_h264_webrtc
```

### 方法二：先启动 C 程序

```bash
# 终端 1
./luckfox_pico_h264_webrtc

# 终端 2
./h264_receiver
```

两种方法都可以，程序会自动连接。

## 输出

程序会：
1. 实时显示接收统计信息（帧数、帧率、码率）
2. 将 H.264 流保存到 `output.h264` 文件

## 播放录制的视频

```bash
# 使用 ffplay
ffplay output.h264

# 使用 VLC
vlc output.h264

# 转换为 MP4
ffmpeg -i output.h264 -c:v copy output.mp4
```

## 输出示例

```
========================================
  Luckfox H.264 Receiver (Test)
========================================
Connecting to: /tmp/luckfox_h264.sock
✓ Connected to luckfox_pico_h264_webrtc
----------------------------------------
✓ Output file: output.h264
========================================
Receiving H.264 stream... (Press Ctrl+C to stop)

[Frame #00305] Type: P | Size:   4521 bytes | FPS:  30.2 | Bitrate:  2.05 Mbps | I: 11 P: 294
```

按 `Ctrl+C` 停止录制。

## 故障排除

### 提示 "Connection refused"
- 确保 C 程序已经启动
- 检查 `/tmp/luckfox_h264.sock` 文件是否存在

### 收不到数据
- 检查 C 程序是否正常运行
- 查看是否有错误日志
- 尝试重启两个程序

