#ifndef __H264_FRAME_PACKET_H
#define __H264_FRAME_PACKET_H

#include <stdint.h>

// H.264帧数据包头结构
// 用于在C程序和Go程序之间传输H.264帧数据
typedef struct {
    uint32_t magic;        // 魔数: 0x48323634 ("H264")
    uint64_t timestamp;    // 时间戳（微秒）
    uint32_t frame_type;   // 帧类型: 0=P帧, 1=I帧
    uint32_t data_len;     // H.264数据长度（字节）
    uint32_t sequence;     // 帧序号
    uint8_t  reserved[8];  // 保留字段，用于未来扩展
} __attribute__((packed)) H264FrameHeader;

// 协议常量定义
#define FRAME_MAGIC   0x48323634  // "H264"的十六进制表示
#define FRAME_TYPE_P  0           // P帧（预测帧）
#define FRAME_TYPE_I  1           // I帧（关键帧）

#endif

