/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <unistd.h>

#include "ot_type.h"
#include "sample_comm.h"
#include "scene.h"

#define SCENE_WAIT_EFFECT_TIME 5000

static ot_scene_param g_scene_param;
static ot_scene_video_mode g_video_mode = {0};
static td_bool g_scene_init = TD_FALSE;

td_s32 scene_init(const td_char *dir_name, td_u32 route_num)
{
    td_s32 ret = TD_SUCCESS;
    ret = ot_scene_create_param(dir_name, &g_scene_param, &g_video_mode);
    if (ret != TD_SUCCESS) {
        sample_print("scene_init createParam failed\n");
        return TD_FAILURE;
    }

    ret = ot_scene_init(&g_scene_param);
    if (ret != TD_SUCCESS) {
        sample_print("ot_scene_init failed\n");
        return TD_FAILURE;
    }

    td_s32 mode_index = route_num == 1 ? 0 : 1;
    ret = ot_scene_set_scene_mode(&(g_video_mode.video_mode[mode_index]));
    if (ret != TD_SUCCESS) {
        sample_print("set scene mode failed\n");
        return TD_FAILURE;
    }
    g_scene_init = TD_TRUE;
    return TD_SUCCESS;
}

td_s32 scene_deinit(td_void)
{
    if (!g_scene_init) {
        return TD_SUCCESS;
    }
    g_scene_init = TD_FALSE;
    return ot_scene_deinit();
}

td_s32 scene_resume(td_void)
{
    td_s32 ret;

    if (!g_scene_init) {
        return TD_SUCCESS;
    }
    ret = ot_scene_pause(TD_FALSE);
    usleep(SCENE_WAIT_EFFECT_TIME);
    return ret;
}

td_s32 scene_pause(td_void)
{
    td_s32 ret;

    if (!g_scene_init) {
        return TD_SUCCESS;
    }
    ret = ot_scene_pause(TD_TRUE);
    usleep(SCENE_WAIT_EFFECT_TIME);
    return ret;
}

td_s32 scene_run_once(td_void)
{
    if (!g_scene_init) {
        return TD_SUCCESS;
    }
    return ot_scene_run_once();
}

td_s32 scene_set_scene_mode(td_u32 mode)
{
    td_s32 ret;

    if (!g_scene_init) {
        return TD_SUCCESS;
    }
    if (mode >= SCENE_MAX_VIDEOMODE) {
        return TD_FAILURE;
    }
    ret = ot_scene_set_scene_mode(&(g_video_mode.video_mode[mode]));
    usleep(SCENE_WAIT_EFFECT_TIME);
    return ret;
}
