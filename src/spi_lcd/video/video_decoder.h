/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
 
#ifndef AV_VIDEO_DECODER_H
#define AV_VIDEO_DECODER_H

#include "vdec_common.h"

td_s32 av_vdec_init(td_void);

td_s32 av_vdec_deinit(td_void);

td_s32 av_vdec_create(td_void **vdec_hdl, ot_payload_type vdec_type, const av_vdec_attr *vdec_attr);

td_s32 av_vdec_destroy(const td_void *vdec_hdl);

td_s32 av_vdec_send_stream(const td_void *vdec_hdl, const av_vdec_stream *vdec_stream);

td_s32 av_vdec_get_frame(const td_void *vdec_hdl, ot_video_frame_info *video_frame_info);

td_s32 av_vdec_release_frame(const td_void *vdec_hdl, const ot_video_frame_info *video_frame_info);

td_s32 av_vdec_start(const td_void *vdec_hdl);

td_s32 av_vdec_stop(const td_void *vdec_hdl);

#endif /* AV_VDEC_H */
