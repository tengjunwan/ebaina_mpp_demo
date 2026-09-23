/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "vdec_mng.h"

#include <pthread.h>
#include "avplay_common.h"

typedef struct {
    ot_vdec_chn vdec_chn;
    td_bool is_used;
} vdec_mng_chn_status;

#ifndef VDEC_MAX_CHN_NUM
#define VDEC_MAX_CHN_NUM  1
#endif

static vdec_mng_chn_status g_vdec_chn_stat_list[VDEC_MAX_CHN_NUM];
static pthread_mutex_t g_vdec_list_lock;
static td_bool g_native_vdec_mng_inited = TD_FALSE;

td_void vdec_mng_init(td_void)
{
    if (g_native_vdec_mng_inited) {
        return;
    }
    for (td_s32 i = 0; i < VDEC_MAX_CHN_NUM; i++) {
        g_vdec_chn_stat_list[i].is_used = TD_FALSE;
        g_vdec_chn_stat_list[i].vdec_chn = i;
    }
    (td_void)pthread_mutex_init(&g_vdec_list_lock, NULL);
    g_native_vdec_mng_inited = TD_TRUE;
}

td_void vdec_mng_deinit(td_void)
{
    for (td_s32 i = 0; i < VDEC_MAX_CHN_NUM; i++) {
        if (g_vdec_chn_stat_list[i].is_used) {
            AV_TRACE("vdec channel %d not be freed err\n", g_vdec_chn_stat_list[i].vdec_chn);
        }
    }
    (td_void)pthread_mutex_destroy(&g_vdec_list_lock);
}

td_s32 vdec_mng_get_free_handle(ot_vdec_chn *vdec_chn)
{
    if (vdec_chn == NULL) {
        AV_TRACE("vdec_chn is null\n");
        return TD_FAILURE;
    }
    if (!g_native_vdec_mng_inited) {
        AV_TRACE("vdec mng have not been inited\n");
        return TD_FAILURE;
    }
    AVPLAY_LOCK(g_vdec_list_lock);
    for (td_s32 i = 0; i < VDEC_MAX_CHN_NUM; i++) {
        if (!g_vdec_chn_stat_list[i].is_used) {
            g_vdec_chn_stat_list[i].is_used = TD_TRUE;
            *vdec_chn = g_vdec_chn_stat_list[i].vdec_chn;
            AVPLAY_UNLOCK(g_vdec_list_lock);
            return TD_SUCCESS;
        }
    }
    AVPLAY_UNLOCK(g_vdec_list_lock);
    AV_TRACE("no free vdec channel\n");
    return TD_FAILURE;
}

td_s32 vdec_mng_free_handle(ot_vdec_chn vdec_chn)
{
    int i;
    if (!g_native_vdec_mng_inited) {
        AV_TRACE("vdec mng have not been inited\n");
        return TD_FAILURE;
    }
    AVPLAY_LOCK(g_vdec_list_lock);
    for (i = 0; i < VDEC_MAX_CHN_NUM; i++) {
        if (g_vdec_chn_stat_list[i].vdec_chn == vdec_chn) {
            g_vdec_chn_stat_list[i].is_used = TD_FALSE;
            AVPLAY_UNLOCK(g_vdec_list_lock);
            return TD_SUCCESS;
        }
    }
    AVPLAY_UNLOCK(g_vdec_list_lock);
    AV_TRACE("freehandle input vdec handle invalid : %d \n", (td_u32)vdec_chn);
    return TD_FAILURE;
}
