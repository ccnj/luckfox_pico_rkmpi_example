/*****************************************************************************
* | Author      :   Luckfox team
* | Function    :   H.264帧通过Unix Socket发送到Go程序
* | Info        :
*
*----------------
* | This version:   V1.0
* | Date        :   2024-12-26
* | Info        :   首次创建
*
******************************************************************************/

#include "h264_socket_sender.h"
#include "h264_frame_packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int h264_socket_init(H264SocketSender *sender, const char *socket_path) {
    memset(sender, 0, sizeof(H264SocketSender));
    strncpy(sender->socket_path, socket_path, sizeof(sender->socket_path) - 1);
    
    // 删除旧的socket文件（如果存在）
    unlink(socket_path);
    
    // 创建Unix domain socket
    sender->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sender->socket_fd < 0) {
        perror("socket create failed");
        return -1;
    }
    
    // 设置地址
    memset(&sender->server_addr, 0, sizeof(sender->server_addr));
    sender->server_addr.sun_family = AF_UNIX;
    strncpy(sender->server_addr.sun_path, socket_path, 
            sizeof(sender->server_addr.sun_path) - 1);
    
    // 绑定socket
    if (bind(sender->socket_fd, 
             (struct sockaddr *)&sender->server_addr,
             sizeof(sender->server_addr)) < 0) {
        perror("bind failed");
        close(sender->socket_fd);
        return -1;
    }
    
    // 开始监听
    if (listen(sender->socket_fd, 1) < 0) {
        perror("listen failed");
        close(sender->socket_fd);
        return -1;
    }
    
    printf("H264 Socket Server listening on %s\n", socket_path);
    printf("Waiting for Go WebRTC client to connect...\n");
    
    return 0;
}

int h264_socket_wait_client(H264SocketSender *sender) {
    printf("Accepting connection...\n");
    sender->client_fd = accept(sender->socket_fd, NULL, NULL);
    if (sender->client_fd < 0) {
        perror("accept failed");
        return -1;
    }
    sender->is_connected = 1;
    printf("Go WebRTC client connected successfully!\n");
    return 0;
}

int h264_socket_send_frame(H264SocketSender *sender,
                          uint8_t *data,
                          uint32_t len,
                          uint64_t timestamp,
                          uint32_t frame_type,
                          uint32_t sequence) {
    if (!sender->is_connected) {
        return -1;
    }
    
    // 构建H.264帧包头
    H264FrameHeader header;
    header.magic = FRAME_MAGIC;
    header.timestamp = timestamp;
    header.frame_type = frame_type;
    header.data_len = len;
    header.sequence = sequence;
    memset(header.reserved, 0, sizeof(header.reserved));
    
    // 发送包头
    ssize_t sent = send(sender->client_fd, &header, sizeof(header), MSG_NOSIGNAL);
    if (sent != sizeof(header)) {
        if (errno == EPIPE || errno == ECONNRESET) {
            printf("Client disconnected (send header)\n");
        } else {
            perror("send header failed");
        }
        sender->is_connected = 0;
        close(sender->client_fd);
        sender->client_fd = -1;
        return -1;
    }
    
    // 发送H.264数据
    size_t total_sent = 0;
    while (total_sent < len) {
        sent = send(sender->client_fd, data + total_sent, len - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                printf("Client disconnected (send data)\n");
            } else {
                perror("send data failed");
            }
            sender->is_connected = 0;
            close(sender->client_fd);
            sender->client_fd = -1;
            return -1;
        }
        total_sent += sent;
    }
    
    return 0;
}

void h264_socket_close(H264SocketSender *sender) {
    if (sender->client_fd > 0) {
        close(sender->client_fd);
        sender->client_fd = -1;
    }
    if (sender->socket_fd > 0) {
        close(sender->socket_fd);
        sender->socket_fd = -1;
    }
    unlink(sender->socket_path);
    sender->is_connected = 0;
    printf("H264 Socket closed\n");
}

