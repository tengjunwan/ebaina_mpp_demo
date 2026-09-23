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

static void ot_stream_eu_select_layer_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint16_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->select_layer_get) {
            v = get_ot_stream()->mpi_eu_ops->select_layer_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_CUR:
            resp->data[0] = v & 0xff;
            resp->data[1] = (v >> SHIFT_8BIT) & 0xff;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_profile_toolset_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint64_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->profile_toolset_get) {
            v = get_ot_stream()->mpi_eu_ops->profile_toolset_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_5;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint64_t));
            resp->length = OT_UVC_RESP_LENGTH_5;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_5;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_5;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_5;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_video_resolution_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->video_resolution_get) {
            v = get_ot_stream()->mpi_eu_ops->video_resolution_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            resp->data[0x0] = v & 0xff;
            resp->data[0x1] = (v >> SHIFT_8BIT) & 0xff;
            resp->data[0x2] = (v >> SHIFT_16BIT) & 0xff;
            resp->data[0x3] = (v >> SHIFT_24BIT) & 0xff;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_min_frame_interval_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->min_frame_interval_get) {
            v = get_ot_stream()->mpi_eu_ops->min_frame_interval_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_slice_mode_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->slice_mode_get) {
            v = get_ot_stream()->mpi_eu_ops->slice_mode_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_rate_ctrl_mode_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint8_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->rate_ctrl_mode_get) {
            v = get_ot_stream()->mpi_eu_ops->rate_ctrl_mode_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_CUR:
            resp->data[0] = v;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_average_bitrate_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->average_bitrate_get) {
            v = get_ot_stream()->mpi_eu_ops->average_bitrate_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_cpb_size_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->cpb_size_get) {
            v = get_ot_stream()->mpi_eu_ops->cpb_size_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_peak_bit_rate_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->peak_bit_rate_get) {
            v = get_ot_stream()->mpi_eu_ops->peak_bit_rate_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_quantization_params_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint64_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->quantization_params_get) {
            v = get_ot_stream()->mpi_eu_ops->quantization_params_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, OT_UVC_RESP_LENGTH_6, &v, OT_UVC_RESP_LENGTH_6);
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_6;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_sync_ref_frame_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint32_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->sync_ref_frame_get) {
            v = get_ot_stream()->mpi_eu_ops->sync_ref_frame_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint32_t));
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_ltr_buffer_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint16_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->ltr_buffer_get) {
            v = get_ot_stream()->mpi_eu_ops->ltr_buffer_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint16_t));
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_ltr_picture_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint16_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->ltr_picture_get) {
            v = get_ot_stream()->mpi_eu_ops->ltr_picture_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint16_t));
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_ltr_validation_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint16_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->ltr_validation_get) {
            v = get_ot_stream()->mpi_eu_ops->ltr_validation_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint16_t));
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_level_idc_limit_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint8_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->level_idc_limit_get) {
            v = get_ot_stream()->mpi_eu_ops->level_idc_limit_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_CUR:
            resp->data[0] = v;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_sei_payloadtype_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint64_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->sei_payloadtype_get) {
            v = get_ot_stream()->mpi_eu_ops->sei_payloadtype_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_8;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint64_t));
            resp->length = OT_UVC_RESP_LENGTH_8;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_8;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_8;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_8;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_8;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_qp_range_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint16_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->qp_range_get) {
            v = get_ot_stream()->mpi_eu_ops->qp_range_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint16_t));
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_MIN:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_MAX:
            resp->data[0] = 0xf;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_priority_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint8_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->priority_get) {
            v = get_ot_stream()->mpi_eu_ops->priority_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_CUR:
            resp->data[0] = v;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_start_or_stop_layer_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint8_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->start_or_stop_layer_get) {
            v = get_ot_stream()->mpi_eu_ops->start_or_stop_layer_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_CUR:
            resp->data[0] = v;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x3;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x8;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void ot_stream_eu_error_resiliency_ctrl(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    uint16_t v = 0x5;
    if (get_ot_stream()->streaming[dev->dev_no] == 1) {
        if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->error_resiliency_get) {
            v = get_ot_stream()->mpi_eu_ops->error_resiliency_get(dev->dev_no);
        }
    }

    dev->request_error_code.data[0] = 0x00;
    dev->request_error_code.length = 1;

    switch (req) {
        case OT_UVC_SET_CUR:
            resp->data[0] = 0x0;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_CUR:
            (td_void)memcpy_s(resp->data, sizeof(resp->data), &v, sizeof(uint16_t));
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_DEF:
            resp->data[0] = 0x4;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_RES:
            resp->data[0] = 0x1;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        default:
            resp->length = -EL2HLT;
            dev->request_error_code.data[0] = 0x07;
            dev->request_error_code.length = 1;
            break;
    }
}

static void do_eu_ctrl_default(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp)
{
    ot_stream_event_default_control(req, resp);
    resp->length = -EL2HLT;
    dev->request_error_code.data[0] = 0x06;
    dev->request_error_code.length = 1;
}

typedef void (*do_eu_ctrl)(struct ot_uvc_dev *dev, uint8_t req, struct uvc_request_data *resp);
static do_eu_ctrl g_eu_ctrl_list[] = {
    [OT_UVC_EU_CONTROL_UNDEFINED] = do_eu_ctrl_default,
    [OT_UVC_EU_SELECT_LAYER_CONTROL] = ot_stream_eu_select_layer_ctrl,
    [OT_UVC_EU_PROFILE_TOOLSET_CONTROL] = ot_stream_eu_profile_toolset_ctrl,
    [OT_UVC_EU_VIDEO_RESOLUTION_CONTROL] = ot_stream_eu_video_resolution_ctrl,
    [OT_UVC_EU_MIN_FRAME_INTERVAL_CONTROL] = ot_stream_eu_min_frame_interval_ctrl,
    [OT_UVC_EU_SLICE_MODE_CONTROL] = ot_stream_eu_slice_mode_ctrl,
    [OT_UVC_EU_RATE_CONTROL_MODE_CONTROL] = ot_stream_eu_rate_ctrl_mode_ctrl,
    [OT_UVC_EU_AVERAGE_BITRATE_CONTROL] = ot_stream_eu_average_bitrate_ctrl,
    [OT_UVC_EU_CPB_SIZE_CONTROL] = ot_stream_eu_cpb_size_ctrl,
    [OT_UVC_EU_PEAK_BIT_RATE_CONTROL] = ot_stream_eu_peak_bit_rate_ctrl,
    [OT_UVC_EU_QUANTIZATION_PARAMS_CONTROL] = ot_stream_eu_quantization_params_ctrl,
    [OT_UVC_EU_SYNC_REF_FRAME_CONTROL] = ot_stream_eu_sync_ref_frame_ctrl,
    [OT_UVC_EU_LTR_BUFFER_CONTROL] = ot_stream_eu_ltr_buffer_ctrl,
    [OT_UVC_EU_LTR_PICTURE_CONTROL] = ot_stream_eu_ltr_picture_ctrl,
    [OT_UVC_EU_LTR_VALIDATION_CONTROL] = ot_stream_eu_ltr_validation_ctrl,
    [OT_UVC_EU_LEVEL_IDC_LIMIT_CONTROL] = ot_stream_eu_level_idc_limit_ctrl,
    [OT_UVC_EU_SEI_PAYLOADTYPE_CONTROL] = ot_stream_eu_sei_payloadtype_ctrl,
    [OT_UVC_EU_QP_RANGE_CONTROL] = ot_stream_eu_qp_range_ctrl,
    [OT_UVC_EU_PRIORITY_CONTROL] = ot_stream_eu_priority_ctrl,
    [OT_UVC_EU_START_OR_STOP_LAYER_CONTROL] = ot_stream_eu_start_or_stop_layer_ctrl,
    [OT_UVC_EU_ERROR_RESILIENCY_CONTROL] = ot_stream_eu_error_resiliency_ctrl,
    NULL,
};

void ot_stream_event_eu_control(struct ot_uvc_dev *dev, uint8_t req, uint8_t unit_id, uint8_t cs,
    struct uvc_request_data *resp)
{
    if (dev == NULL || resp == NULL) {
        err("dev or resp is NULL\n");
        return;
    }
    if (unit_id != OT_UVC_ENCODING_UNIT_ID) {
        return;
    }

    if (cs <= OT_UVC_EU_CONTROL_UNDEFINED || cs > OT_UVC_EU_ERROR_RESILIENCY_CONTROL) {
        g_eu_ctrl_list[OT_UVC_EU_CONTROL_UNDEFINED](dev, req, resp);
        return;
    }

    g_eu_ctrl_list[cs](dev, req, resp);
}

static void ot_stream_eu_select_layer_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->select_layer_set) {
        get_ot_stream()->mpi_eu_ops->select_layer_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_profile_toolset_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t profile;
    uint16_t constrained_toolset;
    uint16_t settings;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    profile = data->data[0] + (data->data[1] << SHIFT_8BIT);
    constrained_toolset = data->data[0x2] + (data->data[0x3] << SHIFT_8BIT);
    settings = data->data[0x4] + (data->data[0x5] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->profile_toolset_set) {
        get_ot_stream()->mpi_eu_ops->profile_toolset_set(dev->dev_no, profile, constrained_toolset, settings);
    }
}

static void ot_stream_eu_video_resolution_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t width;
    uint16_t height;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    width = data->data[0] + (data->data[1] << SHIFT_8BIT);
    height = data->data[0x2] + (data->data[0x3] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->video_resolution_set) {
        get_ot_stream()->mpi_eu_ops->video_resolution_set(dev->dev_no, width, height);
    }
}

static void ot_stream_eu_min_frame_interval_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint32_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT) +
                    (data->data[0x2] << SHIFT_16BIT) + (data->data[0x3] << SHIFT_24BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->min_frame_interval_set) {
        get_ot_stream()->mpi_eu_ops->min_frame_interval_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_slice_mode_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t slice_mode;
    uint16_t slice_config;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    slice_mode = data->data[0] + (data->data[1] << SHIFT_8BIT);
    slice_config = data->data[0x2] + (data->data[0x3] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->slice_mode_set) {
        get_ot_stream()->mpi_eu_ops->slice_mode_set(dev->dev_no, slice_mode, slice_config);
    }
}

static void ot_stream_eu_rate_ctrl_mode_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->rate_ctrl_mode_set) {
        get_ot_stream()->mpi_eu_ops->rate_ctrl_mode_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_average_bitrate_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint32_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT) +
                    (data->data[0x2] << SHIFT_16BIT) + (data->data[0x3] << SHIFT_24BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->average_bitrate_set) {
        get_ot_stream()->mpi_eu_ops->average_bitrate_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_cpb_size_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint32_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT) +
                    (data->data[0x2] << SHIFT_16BIT) + (data->data[0x3] << SHIFT_24BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->cpb_size_set) {
        get_ot_stream()->mpi_eu_ops->cpb_size_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_peak_bit_rate_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint32_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT) +
                    (data->data[0x2] << SHIFT_16BIT) + (data->data[0x3] << SHIFT_24BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->peak_bit_rate_set) {
        get_ot_stream()->mpi_eu_ops->peak_bit_rate_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_quantization_params_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t qp_prime_i;
    uint16_t qp_prime_p;
    uint16_t qp_prime_b;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    qp_prime_i = data->data[0] + (data->data[1] << SHIFT_8BIT);
    qp_prime_p = data->data[0x2] + (data->data[0x3] << SHIFT_8BIT);
    qp_prime_b = data->data[0x4] + (data->data[0x5] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->quantization_params_set) {
        get_ot_stream()->mpi_eu_ops->quantization_params_set(dev->dev_no, qp_prime_i, qp_prime_p, qp_prime_b);
    }
}

static void ot_stream_eu_sync_ref_frame_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t sync_frame_type;
    uint8_t sync_frame_interval;
    uint8_t gradual_decoder_refresh;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    sync_frame_type = data->data[0];
    sync_frame_interval = data->data[1];
    gradual_decoder_refresh = data->data[0x2];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->sync_ref_frame_set) {
        get_ot_stream()->mpi_eu_ops->sync_ref_frame_set(dev->dev_no, sync_frame_type, sync_frame_interval,
            gradual_decoder_refresh);
    }
}

static void ot_stream_eu_ltr_buffer_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t ltr_buffer_size;
    uint8_t ltr_encoder_control;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    ltr_buffer_size = data->data[0];
    ltr_encoder_control = data->data[1];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->ltr_buffer_set) {
        get_ot_stream()->mpi_eu_ops->ltr_buffer_set(dev->dev_no, ltr_buffer_size, ltr_encoder_control);
    }
}

static void ot_stream_eu_ltr_picture_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t put_at_position_in_ltrbuffer;
    uint8_t ltr_mode;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    put_at_position_in_ltrbuffer = data->data[0];
    ltr_mode = data->data[1];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->ltr_picture_set) {
        get_ot_stream()->mpi_eu_ops->ltr_picture_set(dev->dev_no, put_at_position_in_ltrbuffer, ltr_mode);
    }
}

static void ot_stream_eu_ltr_validation_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->ltr_validation_set) {
        get_ot_stream()->mpi_eu_ops->ltr_validation_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_level_idc_limit_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->level_idc_limit_set) {
        get_ot_stream()->mpi_eu_ops->level_idc_limit_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_sei_payloadtype_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint64_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    (td_void)memcpy_s(&v, sizeof(uint64_t), data->data, sizeof(uint64_t));

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->sei_payloadtype_set) {
        get_ot_stream()->mpi_eu_ops->sei_payloadtype_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_qp_range_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t min_qp;
    uint8_t max_qp;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    min_qp = data->data[0];
    max_qp = data->data[1];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->qp_range_set) {
        get_ot_stream()->mpi_eu_ops->qp_range_set(dev->dev_no, min_qp, max_qp);
    }
}

static void ot_stream_eu_priority_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->priority_set) {
        get_ot_stream()->mpi_eu_ops->priority_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_start_or_stop_layer_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint8_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0];

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->start_or_stop_layer_set) {
        get_ot_stream()->mpi_eu_ops->start_or_stop_layer_set(dev->dev_no, v);
    }
}

static void ot_stream_eu_error_resiliency_set(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    uint16_t v;

    if (get_ot_stream()->streaming[dev->dev_no] != 1) {
        return;
    }

    v = data->data[0] + (data->data[1] << SHIFT_8BIT);

    if (get_ot_stream()->mpi_eu_ops && get_ot_stream()->mpi_eu_ops->error_resiliency_set) {
        get_ot_stream()->mpi_eu_ops->error_resiliency_set(dev->dev_no, v);
    }
}

static void eu_data_do_nothing(struct ot_uvc_dev *dev, const struct uvc_request_data *data)
{
    return;
}

typedef void (*do_eu_data)(struct ot_uvc_dev *dev, const struct uvc_request_data *data);
static do_eu_data g_eu_data_list[] = {
    [OT_UVC_EU_CONTROL_UNDEFINED] = eu_data_do_nothing,
    [OT_UVC_EU_SELECT_LAYER_CONTROL] = ot_stream_eu_select_layer_set,
    [OT_UVC_EU_PROFILE_TOOLSET_CONTROL] = ot_stream_eu_profile_toolset_set,
    [OT_UVC_EU_VIDEO_RESOLUTION_CONTROL] = ot_stream_eu_video_resolution_set,
    [OT_UVC_EU_MIN_FRAME_INTERVAL_CONTROL] = ot_stream_eu_min_frame_interval_set,
    [OT_UVC_EU_SLICE_MODE_CONTROL] = ot_stream_eu_slice_mode_set,
    [OT_UVC_EU_RATE_CONTROL_MODE_CONTROL] = ot_stream_eu_rate_ctrl_mode_set,
    [OT_UVC_EU_AVERAGE_BITRATE_CONTROL] = ot_stream_eu_average_bitrate_set,
    [OT_UVC_EU_CPB_SIZE_CONTROL] = ot_stream_eu_cpb_size_set,
    [OT_UVC_EU_PEAK_BIT_RATE_CONTROL] = ot_stream_eu_peak_bit_rate_set,
    [OT_UVC_EU_QUANTIZATION_PARAMS_CONTROL] = ot_stream_eu_quantization_params_set,
    [OT_UVC_EU_SYNC_REF_FRAME_CONTROL] = ot_stream_eu_sync_ref_frame_set,
    [OT_UVC_EU_LTR_BUFFER_CONTROL] = ot_stream_eu_ltr_buffer_set,
    [OT_UVC_EU_LTR_PICTURE_CONTROL] = ot_stream_eu_ltr_picture_set,
    [OT_UVC_EU_LTR_VALIDATION_CONTROL] = ot_stream_eu_ltr_validation_set,
    [OT_UVC_EU_LEVEL_IDC_LIMIT_CONTROL] = ot_stream_eu_level_idc_limit_set,
    [OT_UVC_EU_SEI_PAYLOADTYPE_CONTROL] = ot_stream_eu_sei_payloadtype_set,
    [OT_UVC_EU_QP_RANGE_CONTROL] = ot_stream_eu_qp_range_set,
    [OT_UVC_EU_PRIORITY_CONTROL] = ot_stream_eu_priority_set,
    [OT_UVC_EU_START_OR_STOP_LAYER_CONTROL] = ot_stream_eu_start_or_stop_layer_set,
    [OT_UVC_EU_ERROR_RESILIENCY_CONTROL] = ot_stream_eu_error_resiliency_set,
    NULL,
};

void ot_stream_event_eu_data(struct ot_uvc_dev *dev, uint32_t unit_id, uint32_t control, struct uvc_request_data *data)
{
    if (dev == NULL || data == NULL) {
        err("dev or data is NULL\n");
        return;
    }
    if (unit_id != OT_UVC_ENCODING_UNIT_ID) {
        return;
    }

    if (control > OT_UVC_EU_ERROR_RESILIENCY_CONTROL) {
        err("invalid event type!\n");
        return;
    }

    g_eu_data_list[control](dev, data);
}