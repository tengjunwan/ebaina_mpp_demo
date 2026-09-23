/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef AVPLAY_COMMON_H
#define AVPLAY_COMMON_H

#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mw_type.h"
#include "securec.h"

#define MODULE_NAME_AVPLAY "avplay"

#define MS_SCALE   1000

#define AV_INVALID_PTS (-1)

#define NO_WAIT  0

#define ZERO_ARG  0

#define MAX_MSG_QUEUE_SIZE  50

#define MAX_MSG_QUEUE_PAYLOAD_SIZE  128

#define AVPLAY_SPEED_DECIMAL_PRECISION  1000

// 1024 * 16 (16k)
#define VIDEO_WIDTH_HEIGHT_MAX 16384

// 268435456 = 256M = 1024 * 1024 * 256
#define AVPLAY_MAX_VBBUFFER_SIZE 268435456

// comes from ffmpeg, more detail pls refer to ffmpeg doc
// avc decoding in ffmepg needs 64 padding bytes for input buffer
#ifndef AV_INPUT_BUFFER_PADDING_SIZE
#define AV_INPUT_BUFFER_PADDING_SIZE 64
#endif

#define AV_UNUSED_ATTR __attribute__((unused))

#define AV_PRINTF(format, ...) printf(format, ##__VA_ARGS__)

td_void AVPLAY_RETURN_VAL_IF_NULL(const td_void *condition);
td_void AVPLAY_LOCK(pthread_mutex_t mutex);
td_void AVPLAY_UNLOCK(pthread_mutex_t mutex);

td_void AV_TRACE(const td_char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* AVPLAY_COMMON_H */
