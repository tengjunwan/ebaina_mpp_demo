/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __SCENE_H__
#define __SCENE_H__

#include "scene_loadparam.h"

td_s32 scene_init(const td_char *dir_name, td_u32 route_num);
td_s32 scene_deinit(td_void);
td_s32 scene_resume(td_void);
td_s32 scene_pause(td_void);
td_s32 scene_run_once(td_void);
td_s32 scene_set_scene_mode(td_u32 mode);
#endif
