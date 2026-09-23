/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <pthread.h>
#include <unistd.h>
#include "ot_defines.h"
#include "log.h"
#include "uvc.h"
#include "camera.h"
#include "securec.h"
#include "ot_uvc.h"

static pthread_t g_uvc_send_data_pid[OT_UVC_MAX_CHN_NUM];

static int uvc_init(uint32_t dev_no)
{
    return 0;
}

static int uvc_open(uint32_t dev_no)
{
    char devpath[64] = {0}; /* 64 max device path len */
    snprintf_truncated_s(devpath, sizeof(devpath), "/dev/video%u", dev_no);

    return open_uvc_device(devpath, dev_no);
}

static int uvc_close(uint32_t dev_no)
{
    return close_uvc_device(dev_no);
}

static void *uvc_send_data_thread(void *p)
{
    int status = 0;
    const td_bool running = TD_TRUE;
    uint32_t dev_no = (uintptr_t)p;

    while (running) {
        if (sample_uvc_get_quit_flag() != 0) {
            return NULL;
        }
        status = run_uvc_data(dev_no);
    }

    rlog("uvc_send_data_thread exit, status: %d.\n", status);

    return NULL;
}

static int uvc_run(uint32_t dev_no)
{
    const td_bool running = TD_TRUE;

    pthread_create(&g_uvc_send_data_pid[dev_no], 0, uvc_send_data_thread, (void *)(uintptr_t)dev_no);

    while (running) {
        if (sample_uvc_get_quit_flag() != 0) {
            break;
        }

        int status = run_uvc_device(dev_no);
        if (status < 0) {
            break;
        }
    }

    pthread_join(g_uvc_send_data_pid[dev_no], NULL);

    return 0;
}

/* ---------------------------------------------------------------------- */

static ot_uvc g_ot_uvc = {
    .init = uvc_init,
    .open = uvc_open,
    .close = uvc_close,
    .run = uvc_run,
};

ot_uvc *get_ot_uvc(void)
{
    return &g_ot_uvc;
}
