/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __KOL_TIMESTAMP_H__
#define __KOL_TIMESTAMP_H__
#include "ot_type.h"
#include "media.h"

typedef struct {
    td_u64 after_suspend;
    td_u64 sensor_restart[MAX_ROUTE_CNT];
    td_u64 frame_pts[MAX_ROUTE_CNT];
    td_u64 run_end_pts[MAX_ROUTE_CNT];
    td_u64 before_osd[MAX_ROUTE_CNT];
    td_u64 after_osd[MAX_ROUTE_CNT];
    td_u64 before_aidetect[MAX_ROUTE_CNT];
    td_u64 after_aidetect[MAX_ROUTE_CNT];
    td_u64 before_wait_venc[MAX_ROUTE_CNT];
    td_u64 after_wait_venc[MAX_ROUTE_CNT];
    td_u64 before_suspend;
} kol_timestamp;

kol_timestamp *kol_get_timestamp(td_void);
td_void set_statistics_switch(td_bool is_open);
td_bool get_statistics_switch(td_void);
td_void kol_show_timestamp(td_u32 route_num);
td_void update_sensor_restart_times(td_u32 route_id);
td_void update_sensor_standby_times(td_u32 route_id);

#endif /* __KOL_TIMESTAMP_H__ */