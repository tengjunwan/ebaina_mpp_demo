/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "stdio.h"
#include "stddef.h"
#include "sample_comm.h"
#include "state_machine.h"

td_void state_machine_init(state_machine_cfg *state_machine, kol_action_map *action_map, kol_event_map *event_map)
{
    if ((state_machine == NULL) || (action_map == NULL) || (event_map == NULL)) {
        sample_print("state machine init input param is null.\n");
        return;
    }

    state_machine->cur_state = START_STATE_BUTT;
    state_machine->next_state = START_STATE_BUTT;
    state_machine->event_map = event_map;
    state_machine->action_map = action_map;
}

td_void state_machine_transfer(state_machine_cfg *state_machine, kol_start_event event_id)
{
    if (state_machine == NULL) {
        sample_print("state machine transfer input param is null.\n");
        return;
    }

    for (td_u32 i = 0; state_machine->event_map[i].event_id < START_EVENT_BUTT; i++) {
        if ((event_id == state_machine->event_map[i].event_id) &&
            (state_machine->cur_state == state_machine->event_map[i].cur_state)) {
            state_machine->next_state = state_machine->event_map[i].next_state;
            return;
        }
    }
}

td_void state_machine_action(state_machine_cfg *state_machine, td_u32 route_num)
{
    if (state_machine == NULL) {
        sample_print("state machine action input param is null.\n");
        return;
    }

    if (state_machine->next_state != START_STATE_BUTT) {
        state_machine->action_map[state_machine->cur_state].exit(route_num, state_machine->next_state);
        state_machine->action_map[state_machine->next_state].enter(route_num, state_machine->cur_state);
        state_machine->cur_state = state_machine->next_state;
        state_machine->next_state = START_STATE_BUTT;
        state_machine->action_map[state_machine->cur_state].running(route_num);
    } else {
        state_machine->action_map[state_machine->cur_state].running(route_num);
    }
}
