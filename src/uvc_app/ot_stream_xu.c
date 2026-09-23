/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
 
#include <pthread.h>
#include "log.h"
#include "ot_stream.h"
#include "frame_cache.h"
#include "uvc.h"
#include "ot_ctrl.h"
#include "securec.h"

static uint32_t xu_demo1_value;

static void ot_stream_xu_vendor_defined_demo1_control(struct ot_uvc_dev *dev, uint8_t req,
    struct uvc_request_data *resp)
{
    switch (req) {
        case OT_UVC_SET_CUR:
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &xu_demo1_value, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0x0] = 0x0;
            resp->data[0x1] = 0x0;
            resp->data[0x2] = 0x0;
            resp->data[0x3] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0x0] = 0xff;
            resp->data[0x1] = 0x0;
            resp->data[0x2] = 0x0;
            resp->data[0x3] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0x0] = 0x4;
            resp->data[0x1] = 0x0;
            resp->data[0x2] = 0x0;
            resp->data[0x3] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x03;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x04;
            resp->data[1] = 0x00;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;

        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

void ot_stream_event_xu_control(struct ot_uvc_dev *dev, uint8_t req, uint8_t unit_id, uint8_t cs,
    struct uvc_request_data *resp)
{
    if (resp == NULL) {
        err("resp is NULL\n");
        return;
    }
    if (unit_id != OT_UVC_EXTENSION_UNIT_ID) {
        return;
    }

    switch (cs) {
        case XU_VENDOR_DEFINED_DEMO1 :
            ot_stream_xu_vendor_defined_demo1_control(dev, req, resp);
            break;
        default:
            ot_stream_event_default_control(req, resp);
            break;
    }
}

static void ot_stream_xu_vendor_defined_demo1_set(const struct uvc_request_data *data)
{
    (td_void)memcpy_s(&xu_demo1_value, sizeof(uint32_t), data->data, sizeof(uint32_t));
    printf("[SET] XU_VENDOR_DEFINED_DEMO1 \n v:0x%x %x %x %x\n", data->data[0x3], data->data[0x2], data->data[1],
        data->data[0]);
}

void ot_stream_event_xu_data(struct ot_uvc_dev *dev, uint32_t unit_id, uint32_t control, struct uvc_request_data *data)
{
    if (unit_id != OT_UVC_EXTENSION_UNIT_ID) {
        return;
    }

    switch (control) {
        case XU_VENDOR_DEFINED_DEMO1:
            ot_stream_xu_vendor_defined_demo1_set(data);
            break;
        default:
            break;
    }
}

