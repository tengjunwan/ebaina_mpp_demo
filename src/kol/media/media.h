/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __MEDIA_H__
#define __MEDIA_H__
#include "sample_comm.h"

#define MAX_ROUTE_CNT 4

#define CHN1_WIDTH 768
#define CHN1_HEIGHT 448

#define FRAME_RATE 15

typedef struct {
    sample_sns_type sns_type;
    ot_vi_pipe vi_pipe;
    ot_vpss_grp vpss_grp;
    td_u32 venc_chn_num;
    ot_venc_chn venc_chn[OT_VENC_MAX_CHN_NUM];
    td_u32 vpss_chn_num;
} kol_route_info;

td_s32 media_init(td_void);
td_u32 media_get_route_num(td_void);
td_void media_set_route_num(td_u32 route_num);
kol_route_info *media_get_route_info(td_u32 route_id);
td_void media_deinit(td_void);
td_void media_set_aidetect_mask(td_u32 mask);
td_u32 media_get_aidetect_mask(td_void);
td_u32 media_get_route_by_venc_chn(ot_venc_chn venc_chn);
td_void media_trigger_jpeg(td_void);
#endif