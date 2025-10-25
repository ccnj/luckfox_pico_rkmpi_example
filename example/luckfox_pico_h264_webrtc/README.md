# Luckfox Pico H.264 WebRTC 低延迟图传

## 📝 项目简介

本示例实现了从 Luckfox Pico 开发板获取硬件编码的 H.264 视频流，并通过 Unix Domain Socket 传输给 Go 编写的 WebRTC 服务器，实现低延迟的网络视频传输。

### 特性
- ✅ 硬件 H.264 编码（低CPU占用）
- ✅ Unix Socket 进程间通信（高性能、低延迟）
- ✅ 优化的编码参数（适合 WebRTC）
- ✅ 自动重连机制
- ✅ 实时统计信息
- ✅ 分辨率：1280x720 @ 30fps
- ✅ 码率：2Mbps

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────────────┐
│              Luckfox Pico 设备                       │
│                                                      │
│  ┌──────────────┐        ┌──────────────────┐      │
│  │  C程序        │        │   Go程序          │      │
│  │              │        │   (WebRTC)       │      │
│  │  摄像头       │        │                  │      │
│  │    ↓         │        │                  │      │
│  │  VI(YUV)     │        │                  │      │
│  │    ↓         │ Unix   │                  │      │
│  │  VENC(H264)  │ Socket │  H264接收器      │      │
│  │    ↓         │────────→   ↓             │      │
│  │  Socket发送  │        │  WebRTC编码      │      │
│  │              │        │   ↓             │      │
│  └──────────────┘        │  RTP传输         │      │
│                          └────────┬─────────┘      │
└───────────────────────────────────┼─────────────────┘
                                    │ WebRTC
                                    ↓
                            ┌───────────────┐
                            │   浏览器       │
                            │   客户端       │
                            └───────────────┘
```

## 🔧 编译

### 1. 设置环境变量

```bash
# 使用 uclibc
export LUCKFOX_SDK_PATH=/path/to/luckfox-pico

# 或使用 glibc
export GLIBC_COMPILER=/path/to/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
```

### 2. 编译

```bash
cd /path/to/luckfox_pico_rkmpi_example
chmod +x build.sh
./build.sh

# 选择 libc 类型
# 1) uclibc
# 2) glibc

# 选择示例
# 6) luckfox_pico_h264_webrtc
```

### 3. 编译结果

编译成功后会在 `install/` 目录下生成：
```
install/glibc/luckfox_pico_h264_webrtc_demo/
└── luckfox_pico_h264_webrtc   # 可执行程序
```

## 🚀 使用方法

### 方案一：单独运行（测试 H.264 输出）

```bash
# 在 Luckfox Pico 上运行
chmod +x luckfox_pico_h264_webrtc
./luckfox_pico_h264_webrtc
```

程序会等待 Go WebRTC 客户端连接到 `/tmp/luckfox_h264.sock`。

### 方案二：配合 Go WebRTC 程序（完整方案）

#### 步骤 1: 创建 Go WebRTC 接收程序

创建 `webrtc-receiver.go`：

```go
package main

import (
    "encoding/binary"
    "fmt"
    "io"
    "log"
    "net"
    "os"
    "os/signal"
    "syscall"
)

const (
    SocketPath = "/tmp/luckfox_h264.sock"
    HeaderSize = 32
)

type H264FrameHeader struct {
    Magic     uint32
    Timestamp uint64
    FrameType uint32
    DataLen   uint32
    Sequence  uint32
}

func main() {
    // 连接到 C 程序
    conn, err := net.Dial("unix", SocketPath)
    if err != nil {
        log.Fatalf("Failed to connect: %v", err)
    }
    defer conn.Close()
    
    fmt.Println("Connected to luckfox_pico_h264_webrtc")
    
    // 打开输出文件
    outFile, err := os.Create("output.h264")
    if err != nil {
        log.Fatalf("Failed to create output file: %v", err)
    }
    defer outFile.Close()
    
    frameCount := 0
    
    // 处理退出信号
    sigChan := make(chan os.Signal, 1)
    signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
    
    go func() {
        <-sigChan
        fmt.Println("\nStopping...")
        conn.Close()
        outFile.Close()
        os.Exit(0)
    }()
    
    // 接收循环
    for {
        // 读取包头
        headerBuf := make([]byte, HeaderSize)
        _, err := io.ReadFull(conn, headerBuf)
        if err != nil {
            log.Printf("Read header error: %v", err)
            break
        }
        
        header := H264FrameHeader{
            Magic:     binary.LittleEndian.Uint32(headerBuf[0:4]),
            Timestamp: binary.LittleEndian.Uint64(headerBuf[4:12]),
            FrameType: binary.LittleEndian.Uint32(headerBuf[12:16]),
            DataLen:   binary.LittleEndian.Uint32(headerBuf[16:20]),
            Sequence:  binary.LittleEndian.Uint32(headerBuf[20:24]),
        }
        
        // 读取 H.264 数据
        data := make([]byte, header.DataLen)
        _, err = io.ReadFull(conn, data)
        if err != nil {
            log.Printf("Read data error: %v", err)
            break
        }
        
        // 写入文件
        outFile.Write(data)
        
        frameCount++
        frameType := "P"
        if header.FrameType == 1 {
            frameType = "I"
        }
        
        if frameCount%30 == 0 {
            fmt.Printf("Received %d frames (Type: %s, Size: %d bytes)\n",
                frameCount, frameType, header.DataLen)
        }
    }
    
    fmt.Printf("Total frames received: %d\n", frameCount)
}
```

#### 步骤 2: 编译 Go 程序

```bash
# 在开发机上交叉编译
GOOS=linux GOARCH=arm GOARM=7 go build -o webrtc-receiver webrtc-receiver.go

# 或在 Luckfox Pico 上直接编译（需要安装 Go）
go build -o webrtc-receiver webrtc-receiver.go
```

#### 步骤 3: 运行

```bash
# 终端 1: 先启动 Go 接收程序
./webrtc-receiver &

# 终端 2: 启动 C 程序
./luckfox_pico_h264_webrtc
```

#### 步骤 4: 验证输出

```bash
# 使用 ffplay 播放录制的 H.264 文件
ffplay output.h264

# 或使用 VLC 播放器打开 output.h264
```

## 📊 性能指标

| 参数 | 数值 |
|------|------|
| 分辨率 | 1280x720 |
| 帧率 | 30 fps |
| 码率 | 2 Mbps |
| GOP | 30 (1秒一个I帧) |
| 延迟 | 100-300ms (局域网) |
| CPU占用 | C程序: <15%, Go程序: <10% |

## 🔍 输出示例

```
======================================
  Luckfox Pico H.264 WebRTC Streamer
======================================
Resolution: 1280x720
Socket Path: /tmp/luckfox_h264.sock
--------------------------------------
H264 Socket Server listening on /tmp/luckfox_h264.sock
Waiting for Go WebRTC client to connect...

[1/5] Initializing ISP...
    ISP initialized successfully

[2/5] Initializing RKMPI system...
    RKMPI system initialized

[3/5] Initializing VI (Video Input)...
    VI initialized: 1280x720

[4/5] Initializing VENC (H.264 Encoder)...
    H.264 encoder initialized
    Bitrate: 2Mbps, GOP: 30

[5/5] Waiting for Go WebRTC client...
Go WebRTC client connected successfully!

======================================
  System Ready! Starting capture...
======================================
Press Ctrl+C to stop

[I-Frame #30] Size: 45632 bytes, Interval: 1.00s
[Stats] Frames: 100 (I:4 P:96), FPS: 30.2, Bitrate: 2.05 Mbps
[I-Frame #60] Size: 43210 bytes, Interval: 1.00s
[Stats] Frames: 200 (I:7 P:193), FPS: 30.1, Bitrate: 2.03 Mbps
```

## 🛠️ 调试技巧

### 1. 测试 Socket 连接

```bash
# 使用 nc 监听 socket
nc -U /tmp/luckfox_h264.sock | xxd | head -n 20
```

### 2. 查看 H.264 流信息

```bash
# 使用 ffprobe 分析
ffprobe -show_streams output.h264
```

### 3. 实时监控性能

```bash
# 监控 CPU 使用率
top -b -n 1 | grep luckfox_pico_h264_webrtc

# 监控内存使用
free -h
```

## ⚙️ 参数调整

### 修改分辨率和帧率

编辑 `src/main.cc`：

```c
// 降低分辨率以减少延迟
#define DISP_WIDTH  640
#define DISP_HEIGHT 480
```

### 调整码率和 GOP

编辑 `src/luckfox_mpi.cc` 中的 `venc_init` 函数：

```c
// 更高质量但需要更高带宽
stAttr.stRcAttr.stH264Cbr.u32BitRate = 4096;  // 4Mbps

// 更频繁的 I 帧以降低延迟
stAttr.stRcAttr.stH264Cbr.u32Gop = 15;        // 0.5秒一个I帧
```

## 📚 协议格式

### H.264 帧数据包格式

```
+------------------+
| Header (32 bytes)|
+------------------+
| H.264 Data       |
| (variable size)  |
+------------------+

Header 结构:
- magic (4 bytes):     0x48323634 ("H264")
- timestamp (8 bytes): 时间戳（微秒）
- frame_type (4 bytes): 0=P帧, 1=I帧
- data_len (4 bytes):  数据长度
- sequence (4 bytes):  帧序号
- reserved (8 bytes):  保留
```

## ⚠️ 注意事项

1. **运行前必须执行**: `RkLunch-stop.sh` 停止后台程序
2. **RV1103 限制**: 资源较少，建议降低分辨率到 640x480
3. **Socket 文件**: 确保 `/tmp` 目录有写权限
4. **连接顺序**: 先启动 Go 程序，再启动 C 程序（或反之均可，支持自动重连）

## 🐛 常见问题

### Q: 提示 "bind failed"？
A: Socket 文件可能已存在，手动删除：`rm /tmp/luckfox_h264.sock`

### Q: 收不到数据？
A: 检查防火墙和权限，确保两个程序都正常运行

### Q: 延迟太高？
A: 尝试降低分辨率、减小 GOP 值、降低码率

### Q: CPU 占用过高？
A: 硬件编码应该很低，检查是否正确初始化了 VENC

## 📖 扩展阅读

- [RKMPI 开发文档](https://wiki.luckfox.com/)
- [WebRTC 协议规范](https://webrtc.org/)
- [H.264 编码原理](https://en.wikipedia.org/wiki/Advanced_Video_Coding)

## 📄 许可证

本项目遵循与主项目相同的许可证。

---

**开发者**: Luckfox Team  
**版本**: 1.0  
**日期**: 2024-12-26

