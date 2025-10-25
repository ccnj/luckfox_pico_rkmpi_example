#ifndef __LUCKFOX_MPI_H
#define __LUCKFOX_MPI_H

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
#include <vector>

#include "sample_comm.h"

// 获取当前时间（微秒）
RK_U64 TEST_COMM_GetNowUs();

// VI设备初始化
int vi_dev_init();

// VI通道初始化
int vi_chn_init(int channelId, int width, int height);

// VENC编码器初始化
int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType);

#endif

