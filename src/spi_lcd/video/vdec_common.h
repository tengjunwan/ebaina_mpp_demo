/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef AV_VDEC_COMMON_H
#define AV_VDEC_COMMON_H

#include "ot_type.h"
#include "ot_common.h"
#include "ot_common_video.h"

#define MAX_FF_LIB_NAME_LEN  1024

#define MAX_DECODER_NAME_LEN  16

typedef struct {
    td_u32 len;          /* W, stream len */
    td_u64 pts;          /* W, time stamp */
    td_bool end_of_frame;  /* W, is the end of a frame */
    td_bool end_of_stream; /* W, is the end of all stream */
    td_bool display;     /* W, is the current frame displayed. only valid by VIDEO_MODE_FRAME */
    td_u8 *addr;         /* W, stream address */
} av_vdec_stream;

typedef struct {
    td_u32 max_width;
    td_u32 max_height;
    td_u32 err_recover;
    td_u32 buf_size;
    td_u32 frame_buf_cnt;
    td_void *priv;
    td_bool use_mmz;
} av_vdec_attr;

#define VDEC_ERR_BASE  0x1000

#define VDEC_ERR_STREAM_BUF_FULL (VDEC_ERR_BASE + 1)

#define VDEC_ERR_FRAME_BUF_EMPTY (VDEC_ERR_BASE + 2)

typedef struct {
    ot_payload_type type;        /* R, video type to be decoded */
    td_u32 stream_bufsizes;     /* R, stream buf size */
    td_u32 left_stream_bytes;    /* R, left stream bytes waiting for decode */
    td_u32 left_stream_frames;   /* R, left frames waiting for decode,only valid for H264D_MODE_FRAME */
    td_u32 left_pics;           /* R, pics waiting for output */
    td_bool start_recv_stream;   /* R, had started recv stream? */
    td_u32 recv_stream_frames;   /* R, how many frames of stream has been received. valid when send by frame. */
    td_u32 decode_stream_frames; /* R, how many frames of stream has been decoded. valid when send by frame. */
} av_vdec_status;

typedef enum {
    AV_VDEC_CMD_BUTT,
} av_vdec_cmd;

typedef struct {
    ot_payload_type type;

    td_char dec_name[MAX_DECODER_NAME_LEN];

    td_s32 (*pfn_create)(td_void **vdec_hdl, const av_vdec_attr *vdec_attr);

    td_s32 (*pfn_destroy)(const td_void *vdec_hdl);

    td_s32 (*pfn_start)(const td_void *vdec_hdl);

    td_s32 (*pfn_stop)(const td_void *vdec_hdl);

    td_s32 (*pfn_send_stream)(const td_void *vdec_hdl, const av_vdec_stream *vdec_stream);

    td_s32 (*pfn_get_frame)(const td_void *vdec_hdl, ot_video_frame_info *vdec_stream);

    td_s32 (*pfn_release_frame)(const td_void *vdec_hdl, const ot_video_frame_info *video_frame_info);
} av_video_decoder;

#endif /* AV_VDEC_COMMON_H */
