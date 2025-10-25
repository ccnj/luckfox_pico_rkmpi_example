/*****************************************************************************
* | Author      :   Luckfox team
* | Function    :   H.264硬件编码 + Unix Socket传输到Go WebRTC程序
* | Info        :   基于RKMPI实现低延迟视频采集和H.264编码
*
*----------------
* | This version:   V1.0
* | Date        :   2024-12-26
* | Info        :   首次创建，专为WebRTC图传优化
*
******************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

#include "luckfox_mpi.h"
#include "h264_socket_sender.h"
#include "h264_frame_packet.h"

// 视频参数配置
#define DISP_WIDTH  1280
#define DISP_HEIGHT 720
#define SOCKET_PATH "/tmp/luckfox_h264.sock"

// 全局变量
static bool g_running = true;
static H264SocketSender g_sender;

// 信号处理函数
void signal_handler(int signo) {
    printf("\nCaught signal %d, exiting gracefully...\n", signo);
    g_running = false;
}

// 判断是否为I帧
static inline uint32_t get_frame_type(VENC_PACK_S *pack) {
    if (pack->DataType.enH264EType == H264E_NALU_IDRSLICE ||
        pack->DataType.enH264EType == H264E_NALU_ISLICE) {
        return FRAME_TYPE_I;
    }
    return FRAME_TYPE_P;
}

int main(int argc, char *argv[]) {
    // 停止后台程序，释放摄像头
    system("RkLunch-stop.sh");
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号
    
    RK_S32 s32Ret = 0;
    int width = DISP_WIDTH;
    int height = DISP_HEIGHT;
    uint32_t frame_seq = 0;
    uint64_t last_i_frame_time = 0;
    
    printf("======================================\n");
    printf("  Luckfox Pico H.264 WebRTC Streamer\n");
    printf("======================================\n");
    printf("Resolution: %dx%d\n", width, height);
    printf("Socket Path: %s\n", SOCKET_PATH);
    printf("--------------------------------------\n");
    
    // 初始化Socket发送器
    if (h264_socket_init(&g_sender, SOCKET_PATH) != 0) {
        printf("ERROR: Failed to init socket sender\n");
        return -1;
    }
    
    // 准备H.264帧结构
    VENC_STREAM_S stFrame;
    stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
    if (!stFrame.pstPack) {
        printf("ERROR: Failed to allocate memory for VENC_PACK_S\n");
        h264_socket_close(&g_sender);
        return -1;
    }
    
    VIDEO_FRAME_INFO_S stViFrame;
    
    printf("\n[1/5] Initializing ISP...\n");
    // ISP初始化
    RK_BOOL multi_sensor = RK_FALSE;
    const char *iq_dir = "/etc/iqfiles";
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
    SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
    SAMPLE_COMM_ISP_Run(0);
    printf("    ISP initialized successfully\n");
    
    printf("\n[2/5] Initializing RKMPI system...\n");
    // RKMPI系统初始化
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        printf("ERROR: RK_MPI_SYS_Init failed\n");
        free(stFrame.pstPack);
        h264_socket_close(&g_sender);
        return -1;
    }
    printf("    RKMPI system initialized\n");
    
    printf("\n[3/5] Initializing VI (Video Input)...\n");
    // VI初始化
    if (vi_dev_init() != 0) {
        printf("ERROR: VI device init failed\n");
        goto cleanup;
    }
    if (vi_chn_init(0, width, height) != 0) {
        printf("ERROR: VI channel init failed\n");
        goto cleanup;
    }
    printf("    VI initialized: %dx%d\n", width, height);
    
    printf("\n[4/5] Initializing VENC (H.264 Encoder)...\n");
    // VENC初始化（H.264编码器）
    RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
    if (venc_init(0, width, height, enCodecType) != 0) {
        printf("ERROR: VENC init failed\n");
        goto cleanup;
    }
    printf("    H.264 encoder initialized\n");
    printf("    Bitrate: 2Mbps, GOP: 30\n");
    
    printf("\n[5/5] Waiting for Go WebRTC client...\n");
    // 等待Go程序连接
    if (h264_socket_wait_client(&g_sender) != 0) {
        printf("ERROR: Failed to wait for client\n");
        goto cleanup;
    }
    
    printf("\n======================================\n");
    printf("  System Ready! Starting capture...\n");
    printf("======================================\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // 统计信息
    uint64_t start_time = TEST_COMM_GetNowUs();
    uint32_t total_frames = 0;
    uint32_t i_frames = 0;
    uint32_t p_frames = 0;
    uint64_t total_bytes = 0;
    
    // 主循环 - 视频采集和编码
    while (g_running) {
        // 获取VI帧（YUV420SP格式）
        s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, 1000);
        if (s32Ret != RK_SUCCESS) {
            if (s32Ret == RK_ERR_VI_TIMEOUT) {
                continue;
            }
            printf("WARNING: RK_MPI_VI_GetChnFrame failed: 0x%x\n", s32Ret);
            usleep(10000);
            continue;
        }
        
        // 送入H.264编码器
        s32Ret = RK_MPI_VENC_SendFrame(0, &stViFrame, 1000);
        if (s32Ret != RK_SUCCESS) {
            printf("WARNING: RK_MPI_VENC_SendFrame failed: 0x%x\n", s32Ret);
            RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
            continue;
        }
        
        // 获取编码后的H.264流
        s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, 1000);
        if (s32Ret == RK_SUCCESS) {
            void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
            uint32_t len = stFrame.pstPack->u32Len;
            uint64_t pts = stFrame.pstPack->u64PTS;
            
            // 判断帧类型
            uint32_t frame_type = get_frame_type(stFrame.pstPack);
            
            // 统计信息
            total_frames++;
            total_bytes += len;
            if (frame_type == FRAME_TYPE_I) {
                i_frames++;
                uint64_t now = TEST_COMM_GetNowUs();
                if (last_i_frame_time > 0) {
                    float interval = (now - last_i_frame_time) / 1000000.0f;
                    printf("[I-Frame #%u] Size: %u bytes, Interval: %.2fs\n", 
                           frame_seq, len, interval);
                }
                last_i_frame_time = now;
            } else {
                p_frames++;
            }
            
            // 通过Socket发送到Go程序
            if (h264_socket_send_frame(&g_sender, 
                                       (uint8_t *)pData, 
                                       len, 
                                       pts, 
                                       frame_type,
                                       frame_seq) != 0) {
                printf("\n[WARNING] Socket disconnected\n");
                printf("Waiting for client to reconnect...\n");
                
                // 等待重新连接
                if (h264_socket_wait_client(&g_sender) != 0) {
                    printf("ERROR: Failed to reconnect\n");
                    RK_MPI_VENC_ReleaseStream(0, &stFrame);
                    RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
                    break;
                }
                printf("Client reconnected!\n\n");
            }
            
            frame_seq++;
            
            // 每100帧输出一次统计信息
            if (total_frames % 100 == 0) {
                uint64_t now = TEST_COMM_GetNowUs();
                float duration = (now - start_time) / 1000000.0f;
                float fps = total_frames / duration;
                float bitrate = (total_bytes * 8) / duration / 1000000.0f;
                
                printf("[Stats] Frames: %u (I:%u P:%u), FPS: %.1f, Bitrate: %.2f Mbps\n",
                       total_frames, i_frames, p_frames, fps, bitrate);
            }
            
            // 释放编码流
            RK_MPI_VENC_ReleaseStream(0, &stFrame);
        }
        
        // 释放VI帧
        RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
    }
    
    // 输出最终统计
    printf("\n======================================\n");
    printf("  Final Statistics\n");
    printf("======================================\n");
    uint64_t end_time = TEST_COMM_GetNowUs();
    float total_duration = (end_time - start_time) / 1000000.0f;
    printf("Total Frames:   %u\n", total_frames);
    printf("I-Frames:       %u\n", i_frames);
    printf("P-Frames:       %u\n", p_frames);
    printf("Duration:       %.2f seconds\n", total_duration);
    printf("Average FPS:    %.2f\n", total_frames / total_duration);
    printf("Total Data:     %.2f MB\n", total_bytes / 1024.0f / 1024.0f);
    printf("Average Bitrate: %.2f Mbps\n", 
           (total_bytes * 8) / total_duration / 1000000.0f);
    printf("======================================\n");
    
cleanup:
    printf("\n[Cleanup] Releasing resources...\n");
    
    // 清理Socket
    h264_socket_close(&g_sender);
    
    // 清理RKMPI资源
    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableDev(0);
    printf("    VI stopped\n");
    
    SAMPLE_COMM_ISP_Stop(0);
    printf("    ISP stopped\n");
    
    RK_MPI_VENC_StopRecvFrame(0);
    RK_MPI_VENC_DestroyChn(0);
    printf("    VENC stopped\n");
    
    free(stFrame.pstPack);
    RK_MPI_SYS_Exit();
    printf("    RKMPI system exited\n");
    
    printf("\n[Done] Exited cleanly. Goodbye!\n");
    return 0;
}

