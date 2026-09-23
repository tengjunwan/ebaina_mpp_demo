/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef OT_STREAM_H
#define OT_STREAM_H

#include <stdint.h>
#include "ot_camera.h"
#include "ot_stream_pu.h"
#include "ot_stream_ct.h"
#include "ot_stream_eu.h"
#include "ot_stream_xu.h"
#include "ot_common_uvc.h"
#include "uvc.h"

#define OT_UVC_RESP_LENGTH_BYTE 1
#define OT_UVC_RESP_LENGTH_HALF_WORD 2
#define OT_UVC_RESP_LENGTH_WORD 4
#define OT_UVC_RESP_LENGTH_5  5
#define OT_UVC_RESP_LENGTH_6  6
#define OT_UVC_RESP_LENGTH_8  8
#define OT_UVC_RESP_LENGTH_10  10
#define OT_UVC_RESP_LENGTH_12  12
#define SHIFT_8BIT 8
#define SHIFT_16BIT 16
#define SHIFT_24BIT 24

typedef struct stream_control_ops {
    int (*init)(void);
    int (*deinit)(void);
    int (*startup)(td_u32 dev_no);
    int (*shutdown)(td_u32 dev_no);
    int (*set_idr)(td_u32 dev_no);
    int (*set_property)(td_u32 dev_no, struct encoder_property *p);
    int (*get_send)(td_u32 dev_no);
} stream_control_ops_st;

typedef struct ot_stream {
    struct stream_control_ops *mpi_sc_ops;
    struct processing_unit_ops *mpi_pu_ops;
    struct camera_terminal_ops *mpi_ct_ops;
    struct encoding_unit_ops *mpi_eu_ops;
    struct extension_unit_ops *mpi_xu_ops;
    int streaming[OT_UVC_MAX_CHN_NUM];
    td_bool exposure_auto_stall;
    td_bool brightness_stall;
    td_bool contrast_auto_stall;
    td_bool hue_auto_stall;
    td_bool gamma_stall;
    td_bool white_balance_component_auto_stall;
} ot_stream;

/* media control functions */
extern int ot_stream_register_mpi_ops(struct stream_control_ops *sc_ops, struct processing_unit_ops *pu_ops,
    struct camera_terminal_ops *ct_ops, struct encoding_unit_ops *eu_ops, struct extension_unit_ops *xu_ops);

extern int ot_stream_set_enc_property(td_u32 dev_no, struct encoder_property *p);
extern int ot_stream_init(void);
extern int ot_stream_deinit(void);
extern int ot_stream_shutdown(td_u32 dev_no);
extern int ot_stream_startup(td_u32 dev_no);
extern int ot_stream_set_enc_idr(td_u32 dev_no);
extern int ot_stream_get_venc_send_uvc(td_u32 dev_no);

extern void ot_stream_event_pu_control(struct ot_uvc_dev *dev, uint8_t req, uint8_t unit_id, uint8_t cs,
    struct uvc_request_data *resp);

extern void ot_stream_event_ct_control(struct ot_uvc_dev *dev, uint8_t req, uint8_t unit_id, uint8_t cs,
    struct uvc_request_data *resp);

extern void ot_stream_event_eu_control(struct ot_uvc_dev *dev, uint8_t req, uint8_t unit_id, uint8_t cs,
    struct uvc_request_data *resp);

extern void ot_stream_event_xu_control(struct ot_uvc_dev *dev, uint8_t req, uint8_t unit_id, uint8_t cs,
    struct uvc_request_data *resp);

extern void ot_stream_event_pu_data(struct ot_uvc_dev *dev, uint32_t unit_id, uint32_t control,
    struct uvc_request_data *data);

extern void ot_stream_event_ct_data(struct ot_uvc_dev *dev, uint32_t unit_id, uint32_t control,
    struct uvc_request_data *data);

extern void ot_stream_event_eu_data(struct ot_uvc_dev *dev, uint32_t unit_id, uint32_t control,
    struct uvc_request_data *data);

extern void ot_stream_event_xu_data(struct ot_uvc_dev *dev, uint32_t unit_id, uint32_t control,
    struct uvc_request_data *data);

ot_stream *get_ot_stream(void);
void ot_stream_event_default_control(uint8_t req, struct uvc_request_data *resp);

#endif
