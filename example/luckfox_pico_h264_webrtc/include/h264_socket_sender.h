#ifndef __H264_SOCKET_SENDER_H
#define __H264_SOCKET_SENDER_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

// H.264 Socket发送器结构
typedef struct {
    int socket_fd;                    // 服务端socket文件描述符
    int client_fd;                    // 客户端连接文件描述符
    struct sockaddr_un server_addr;   // Unix domain socket地址
    char socket_path[256];            // Socket文件路径
    int is_connected;                 // 连接状态标志
} H264SocketSender;

/**
 * 初始化H.264 Socket发送器
 * @param sender Socket发送器结构指针
 * @param socket_path Unix domain socket文件路径
 * @return 0成功，-1失败
 */
int h264_socket_init(H264SocketSender *sender, const char *socket_path);

/**
 * 等待客户端连接（阻塞）
 * @param sender Socket发送器结构指针
 * @return 0成功，-1失败
 */
int h264_socket_wait_client(H264SocketSender *sender);

/**
 * 发送H.264帧数据
 * @param sender Socket发送器结构指针
 * @param data H.264数据指针
 * @param len 数据长度
 * @param timestamp 时间戳（微秒）
 * @param frame_type 帧类型（0=P帧，1=I帧）
 * @param sequence 帧序号
 * @return 0成功，-1失败
 */
int h264_socket_send_frame(H264SocketSender *sender,
                           uint8_t *data,
                           uint32_t len,
                           uint64_t timestamp,
                           uint32_t frame_type,
                           uint32_t sequence);

/**
 * 关闭Socket并清理资源
 * @param sender Socket发送器结构指针
 */
void h264_socket_close(H264SocketSender *sender);

#endif

