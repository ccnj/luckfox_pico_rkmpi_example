# 快速开始指南

## 🎯 5分钟快速测试

### 步骤 1: 编译 C 程序

```bash
cd luckfox_pico_rkmpi_example
./build.sh

# 选择:
# 1) uclibc  (或 2) glibc)
# 6) luckfox_pico_h264_webrtc
```

### 步骤 2: 上传到设备

```bash
# 将编译好的程序上传到 Luckfox Pico
scp install/uclibc/luckfox_pico_h264_webrtc_demo/luckfox_pico_h264_webrtc root@设备IP:/root/

# 也上传 Go 测试程序
cd example/luckfox_pico_h264_webrtc/go_example
GOOS=linux GOARCH=arm GOARM=7 go build -o h264_receiver h264_receiver.go
scp h264_receiver root@设备IP:/root/
```

### 步骤 3: 在设备上运行

```bash
# SSH 登录到设备
ssh root@设备IP

# 终端 1: 启动 Go 接收程序
./h264_receiver &

# 终端 2: 启动 C 采集程序
./luckfox_pico_h264_webrtc

# 等待几秒钟，然后按 Ctrl+C 停止

# 查看录制的视频
ls -lh output.h264
```

### 步骤 4: 在电脑上播放

```bash
# 从设备下载视频文件
scp root@设备IP:/root/output.h264 .

# 播放
ffplay output.h264
```

## ✅ 预期效果

如果一切正常，你应该看到：

```
========================================
  Luckfox Pico H.264 WebRTC Streamer
========================================
Resolution: 1280x720
Socket Path: /tmp/luckfox_h264.sock
...
Go WebRTC client connected successfully!
...
[Stats] Frames: 100 (I:4 P:96), FPS: 30.2, Bitrate: 2.05 Mbps
```

## 🐛 遇到问题？

### 问题 1: 编译失败
```bash
# 确保设置了环境变量
export LUCKFOX_SDK_PATH=/your/path/to/luckfox-pico
```

### 问题 2: 连接失败
```bash
# 确保 socket 文件不存在
rm /tmp/luckfox_h264.sock
```

### 问题 3: 没有视频
```bash
# 检查摄像头是否被占用
RkLunch-stop.sh
```

## 📚 下一步

- 查看 [README.md](README.md) 了解详细文档
- 查看 [go_example/](go_example/) 目录中的 Go 示例代码
- 开发你自己的 WebRTC 服务器

## 🎓 核心代码说明

### C 程序主流程

```c
1. 初始化 Socket 服务器
2. 初始化 ISP (图像信号处理)
3. 初始化 VI (视频输入)
4. 初始化 VENC (H.264 编码器)
5. 等待 Go 客户端连接
6. 循环:
   - 从摄像头获取 YUV 帧
   - 送入硬件编码器
   - 获取 H.264 编码后的数据
   - 通过 Socket 发送给 Go 程序
```

### Go 程序主流程

```go
1. 连接到 C 程序的 Socket
2. 循环:
   - 读取包头 (32 字节)
   - 读取 H.264 数据
   - 写入文件或发送到 WebRTC
```

## 💡 性能提示

- **降低延迟**: 减小分辨率到 640x480
- **提高质量**: 增加码率到 4Mbps
- **节省带宽**: 降低码率到 1Mbps
- **更快的 I 帧**: 减小 GOP 到 15

修改这些参数需要重新编译 C 程序。

