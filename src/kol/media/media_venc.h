/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __MEDIA_VENC_H__
#define __MEDIA_VENC_H__
#include "ot_type.h"
#include "ot_common.h"

#define START_YEAR 1900

typedef td_void (*pfn_notify_stream_finish)(ot_venc_chn venc_chn, td_u64 pts);
td_s32 media_venc_start_get_stream(ot_venc_chn *venc_chn, td_s32 cnt);
td_void media_venc_register_notify(pfn_notify_stream_finish notify_stream_finish);
td_void media_venc_unregister_notify(td_void);
td_void media_stop_venc(ot_venc_chn *venc_chn, td_s32 cnt);
td_void media_venc_enter_normal_notify(td_u32 rout_num);
td_void media_venc_set_push_rtsp(td_bool push_rtsp);
td_bool media_venc_get_push_rtsp();
td_void kol_rtsp_server_start();
td_void kol_rtsp_server_stop();
#endif
