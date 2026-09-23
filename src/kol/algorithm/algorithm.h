/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __ALGORITHM_H__
#define __ALGORITHM_H__
#include "ot_common_video.h"

#define AIDETECT_MAX_CLASS_NUM (8)
#define AIDETECT_MAX_OBJECT_CNT (20)
#define AIDETECT_MAX_RECT_CNT (30)
#define EVEN_PIXEL (2)
#define AIDETECT_RECT_COLOR_MAX (6)
#define CORNER_RECT_LINE_THICK (4)
#define CORNER_RECT_HOR_LEN (80)
#define CORNER_RECT_VER_LEN (40)
#define AIDETECT_MAX_ROUTE (4)

typedef enum {
    AIDETECT_RECT_BLUE = 0X0000FF,
    AIDETECT_RECT_YELLOW = 0XFFFF00,
    AIDETECT_RECT_ORANGE = 0XFFA500,
    AIDETECT_RECT_GREEN = 0X008000,
    AIDETECT_RECT_TEAL = 0X008080,
    AIDETECT_RECT_RED = 0XFF0000,
} AIDETECT_RECT_COLOR;

typedef struct {
    td_u32 rect_cnt;
    ot_rect corner_rect[AIDETECT_MAX_RECT_CNT];
} aidetect_rect;

typedef struct {
    td_u32 width;
    td_u32 height;
} aidetect_resolution;

#ifdef AIDETECT_ENABLE
td_s32 algorithm_aidetect_init(td_u32 route_num);
td_s32 algorithm_aidetect_deinit(td_u32 route_num);
td_s32 algorithm_aidetect_proc(ot_video_frame_info *frame_info, td_u32 route_id, aidetect_rect *corner_rect,
    td_s32 width, td_s32 height);
td_bool algorithm_detected_target(aidetect_rect *corner_rect, td_u32 rect_size);
td_s32 algorithm_set_track_attr(td_u32 route_num, td_bool track_en);
td_s32 algorithm_svc_set_rect(td_u64 pts, ot_size size, aidetect_rect *corner_rect, td_u32 route_id, ot_venc_chn chn);
td_void set_aidetect_mask(td_u32 mask);
#else
static inline td_s32 algorithm_aidetect_init(td_u32 route_num)
{
    return 0;
}

static inline td_s32 algorithm_aidetect_deinit(td_u32 route_num)
{
    return 0;
}

static inline td_s32 algorithm_aidetect_proc(ot_video_frame_info *frame_info, td_u32 route_id,
    aidetect_rect *corner_rect, td_s32 width, td_s32 height)
{
    return 0;
}

static inline td_bool algorithm_detected_target(aidetect_rect *corner_rect, td_u32 rect_size)
{
    return TD_FALSE;
}

static inline td_s32 algorithm_frame_data_draw_rect(aidetect_rect *rect, td_u32 rect_size,
    ot_video_frame_info *frame_info)
{
    return 0;
}

static inline td_s32 algorithm_set_track_attr(td_u32 route_num, td_bool track_en)
{
    return 0;
}

static inline td_s32 algorithm_svc_set_rect(td_u64 pts, ot_size size, aidetect_rect *corner_rect, td_u32 route_id,
    ot_venc_chn chn)
{
    return 0;
}

static inline td_void set_aidetect_mask(td_u32 mask)
{
    return;
}
#endif
#endif
