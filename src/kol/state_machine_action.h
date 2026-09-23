/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __STATE_MACHINE_ACTION_H__
#define __STATE_MACHINE_ACTION_H__

#include "ot_type.h"
#include "state_machine.h"
#include "ss_mpi_syskol.h"

#define TEST_STATE_CNT (7)
#define MAX_ACTION_MAP_CNT (3)
#define MAX_EVENT_MAP_CNT (7)
#define CONVERGENC_TIMEOUT (5)
#define EXPECT_LUMI (100)
#define OSD_TIME_X (50)
#define OSD_TIME_Y (50)
#define OSD_STATE_X (50)
#define OSD_STATE_Y (150)
#define OSD_START_YEAR (1900)
#define AE_VD_TIMEOUT (1000)
#define EXP_VD_TIMEOUT (2000)
#define GET_PIPE_FRAME_TIMEOUT (1000)
#define SEND_PIPE_RAW_TIMEOUT (1000)
#define GET_VPSS_CHN_FRAME_TIMEOUT (5000)
#define VENC_SEND_FRAME_TIMEOUT (2000)
#define RELEASE_LAST_FRAME_VD_TIMEOUT (66)
#define RELEASE_LAST_FRAME_VPSS_TIMEOUT (33)
#define RECORD_STREAM_TIME (15)
#define RECORD_WAIT_TIME (10 * 1000)
#define RECORD_RUN_FRAME_COUNT 30
#define CONVERGENC_RUN_FRAME_COUNT 20
#define LONG_EXPOSURE_TIME 0
#define EXPOSURE_DELAY_NUM 3

td_void set_kol_sleep_mode(td_u32 mode);
ot_syskol_sleep_mode get_kol_sleep_mode(td_void);
td_void set_kol_sleep_time(td_s32 time);
td_void set_kol_iso_threshold(td_u32 threshold);
kol_action_map *get_action_map(td_void);
kol_event_map *get_event_map(td_void);
kol_start_state get_start_state(td_void);
td_void set_start_state(kol_start_state state);
kol_start_state get_single_state(td_void);
td_void set_single_state(kol_start_state state);
kol_start_state get_curr_state(td_void);
td_bool kol_is_need_state_transfer(const state_machine_cfg *state_machine, kol_start_state state);
kol_start_event kol_get_start_event(const state_machine_cfg *state_machine, kol_start_state state);
td_s32 state_machine_resource_init(td_u32 route_num);
td_void state_machine_resource_deinit(td_u32 route_num);
td_void set_mcu_switch(td_bool enable);
#endif
