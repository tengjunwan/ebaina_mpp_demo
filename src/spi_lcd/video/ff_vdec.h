/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef AVPLAYER_FF_VDEC_H
#define AVPLAYER_FF_VDEC_H

#include "vdec_common.h"

td_s32 v_decoder_get_h264(av_video_decoder *decoder);
td_s32 v_decoder_get_jpeg(av_video_decoder *decoder);
td_s32 v_decoder_get_mjpeg(av_video_decoder *decoder);
td_s32 v_decoder_get_h265(av_video_decoder *decoder);
#endif
