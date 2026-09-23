/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

#include "ot_type.h"

typedef enum {
    KOL_STATE = 0,
    NORMAL_CONVERGENC_STATE,
    NORMAL_RECORD_STATE,
    START_STATE_BUTT
} kol_start_state;

typedef enum {
    KOL_TO_CONVERGENC_EVENT = 0,
    KOL_TO_RECORD_EVENT,
    CONVERGENC_TO_KOL_EVENT,
    CONVERGENC_TO_RECORD_EVENT,
    RECORD_TO_KOL_EVENT,
    RECORD_TO_CONVERGENC_EVENT,
    START_EVENT_BUTT
} kol_start_event;

typedef void (*state_running)(td_u32 route_num);
typedef void (*state_action)(td_u32 route_num, kol_start_state state);

typedef struct {
    kol_start_state state_id;
    state_action enter;
    state_running running;
    state_action exit;
} kol_action_map;

typedef struct {
    kol_start_event event_id;
    kol_start_state cur_state;
    kol_start_state next_state;
} kol_event_map;

typedef struct {
    kol_start_state cur_state;
    kol_start_state next_state;
    kol_action_map *action_map;
    kol_event_map *event_map;
} state_machine_cfg;

td_void state_machine_init(state_machine_cfg *state_machine, kol_action_map *action_map, kol_event_map *event_map);
td_void state_machine_transfer(state_machine_cfg *state_machine, kol_start_event event_id);
td_void state_machine_action(state_machine_cfg *state_machine, td_u32 route_num);

#endif