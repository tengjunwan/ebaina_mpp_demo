/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
#include <pthread.h>
#include "log.h"
#include "frame_cache.h"
#include "uvc.h"
#include "ot_ctrl.h"
#include "ot_camera.h"
#include "ot_stream.h"


static struct ot_stream g_ot_stream = {
    .mpi_sc_ops = NULL,
    .mpi_pu_ops = NULL,
    .mpi_ct_ops = NULL,
    .mpi_eu_ops = NULL,
    .mpi_xu_ops = NULL,
    .streaming = {0},
    .exposure_auto_stall = TD_FALSE,
    .brightness_stall = TD_FALSE,
    .contrast_auto_stall = TD_FALSE,
    .hue_auto_stall = TD_FALSE,
    .white_balance_component_auto_stall = TD_FALSE,
};

ot_stream *get_ot_stream(void)
{
    return &g_ot_stream;
}

void ot_stream_event_default_control(uint8_t req, struct uvc_request_data *resp)
{
    switch (req) {
        case OT_UVC_GET_MIN:
            resp->length = OT_UVC_RESP_LENGTH_WORD;
            break;
        case OT_UVC_GET_LEN:
            resp->data[0] = 0x04;
            resp->data[1] = 0x00;
            resp->length = OT_UVC_RESP_LENGTH_HALF_WORD;
            break;
        case OT_UVC_GET_INFO:
            resp->data[0] = 0x03;
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            break;
        case OT_UVC_GET_CUR:
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            resp->data[0] = 0x01;
            break;
        default:
            resp->length = OT_UVC_RESP_LENGTH_BYTE;
            resp->data[0] = 0x06;
            break;
    }
}

/* ******************************************************
 * Stream Control Operation Functions
 * ***************************************************** */
int ot_stream_init(void)
{
    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->init) {
        return get_ot_stream()->mpi_sc_ops->init();
    }

    return 0;
}

int ot_stream_deinit(void)
{
    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->deinit) {
        return get_ot_stream()->mpi_sc_ops->deinit();
    }

    return 0;
}

int ot_stream_startup(td_u32 dev_no)
{
    int ret = 0;

    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->startup) {
        ret = get_ot_stream()->mpi_sc_ops->startup(dev_no);
    }

    if (ret == 0) {
        get_ot_stream()->streaming[dev_no] = 1;
    }
    return ret;
}

int ot_stream_shutdown(td_u32 dev_no)
{
    int ret = 0;

    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->shutdown) {
        ret = get_ot_stream()->mpi_sc_ops->shutdown(dev_no);
    }

    if (ret == 0) {
        get_ot_stream()->streaming[dev_no] = 0;
    }
    return ret;
}

int ot_stream_set_enc_property(td_u32 dev_no, struct encoder_property *p)
{
    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->set_property) {
        return get_ot_stream()->mpi_sc_ops->set_property(dev_no, p);
    }

    return 0;
}

int ot_stream_set_enc_idr(td_u32 dev_no)
{
    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->set_idr) {
        return get_ot_stream()->mpi_sc_ops->set_idr(dev_no);
    }

    return 0;
}

int ot_stream_get_venc_send_uvc(td_u32 dev_no)
{
    if (get_ot_stream()->mpi_sc_ops && get_ot_stream()->mpi_sc_ops->get_send) {
        return get_ot_stream()->mpi_sc_ops->get_send(dev_no);
    }
    return 0;
}

int ot_stream_register_mpi_ops(struct stream_control_ops *sc_ops, struct processing_unit_ops *pu_ops,
    struct camera_terminal_ops *ct_ops, struct encoding_unit_ops *eu_ops, struct extension_unit_ops *xu_ops)
{
    if (sc_ops != NULL) {
        get_ot_stream()->mpi_sc_ops = sc_ops;
    }

    if (pu_ops != NULL) {
        get_ot_stream()->mpi_pu_ops = pu_ops;
    }

    if (ct_ops != NULL) {
        get_ot_stream()->mpi_ct_ops = ct_ops;
    }

    if (eu_ops != NULL) {
        get_ot_stream()->mpi_eu_ops = eu_ops;
    }

    if (xu_ops != NULL) {
        get_ot_stream()->mpi_xu_ops = xu_ops;
    }

    return 0;
}
