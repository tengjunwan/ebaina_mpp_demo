/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include "log.h"
#include "frame_cache.h"
#include "ot_camera.h"
#include "sample_comm.h"
#include "sample_venc.h"
#include "uvc_media.h"
#include "uvc.h"

#define SAMPLE_UVC_MAX_STREAM_NAME_LEN 64

#define SAMPLE_RETURN_CONTINUE  1
#define SAMPLE_RETURN_BREAK     2

static ot_payload_type change_to_mpp_format(uint32_t fcc)
{
    ot_payload_type t;

    switch (fcc) {
        case VIDEO_IMG_FORMAT_YUYV:
        case VIDEO_IMG_FORMAT_NV12:
        case VIDEO_IMG_FORMAT_NV21:
        case VIDEO_IMG_FORMAT_MJPEG:
            t = OT_PT_MJPEG;
            break;

        case VIDEO_IMG_FORMAT_H264:
            t = OT_PT_H264;
            break;

        case VIDEO_IMG_FORMAT_H265:
            t = OT_PT_H265;
            break;

        default:
            t = OT_PT_MJPEG;
            break;
    }

    return t;
}

static ot_pic_size change_to_mpp_wh(encoder_property *p)
{
    ot_size size;
    if (p == NULL) {
        return PIC_1080P;
    }
    size.width = p->width;
    size.height = p->height;

    return sample_comm_sys_get_pic_enum(&size);
}

static td_void set_config_format(td_u32 dev_no, ot_payload_type *format, int idx)
{
    encoder_property property;
    sample_uvc_get_encoder_property(dev_no, &property);

    format[idx] = change_to_mpp_format(property.format);
}

static td_void set_config_wh(td_u32 dev_no, ot_pic_size *wh, int idx)
{
    encoder_property property;
    sample_uvc_get_encoder_property(dev_no, &property);

    wh[idx] = change_to_mpp_wh(&property);
}

td_void set_user_config_format(td_u32 dev_no, ot_payload_type *format, ot_pic_size *wh, td_s32 *c)
{
    if (format == NULL || wh == NULL || c == NULL) {
        err("format or wh, c is NULL\n");
        return;
    }
    set_config_format(dev_no, format, 0);
    set_config_wh(dev_no, wh, 0);
    *c = 1;
}