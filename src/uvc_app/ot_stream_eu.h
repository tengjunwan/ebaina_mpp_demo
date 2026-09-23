/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __STREAM_EU_H__
#define __STREAM_EU_H__

#include <stdint.h>
#include "uvc.h"

#define OT_UVC_EU_CONTROL_UNDEFINED 0x00
#define OT_UVC_EU_SELECT_LAYER_CONTROL 0x01
#define OT_UVC_EU_PROFILE_TOOLSET_CONTROL 0x02
#define OT_UVC_EU_VIDEO_RESOLUTION_CONTROL 0x03
#define OT_UVC_EU_MIN_FRAME_INTERVAL_CONTROL 0x04
#define OT_UVC_EU_SLICE_MODE_CONTROL 0x05
#define OT_UVC_EU_RATE_CONTROL_MODE_CONTROL 0x06
#define OT_UVC_EU_AVERAGE_BITRATE_CONTROL 0x07
#define OT_UVC_EU_CPB_SIZE_CONTROL 0x08
#define OT_UVC_EU_PEAK_BIT_RATE_CONTROL 0x09
#define OT_UVC_EU_QUANTIZATION_PARAMS_CONTROL 0x0a
#define OT_UVC_EU_SYNC_REF_FRAME_CONTROL 0x0b
#define OT_UVC_EU_LTR_BUFFER_CONTROL 0x0c
#define OT_UVC_EU_LTR_PICTURE_CONTROL 0x0d
#define OT_UVC_EU_LTR_VALIDATION_CONTROL 0x0e
#define OT_UVC_EU_LEVEL_IDC_LIMIT_CONTROL 0x0f
#define OT_UVC_EU_SEI_PAYLOADTYPE_CONTROL 0x10
#define OT_UVC_EU_QP_RANGE_CONTROL 0x11
#define OT_UVC_EU_PRIORITY_CONTROL 0x12
#define OT_UVC_EU_START_OR_STOP_LAYER_CONTROL 0x13
#define OT_UVC_EU_ERROR_RESILIENCY_CONTROL 0x14

typedef struct encoding_unit_ops {
    uint16_t (*select_layer_get)(uint32_t dev_no);
    uint64_t (*profile_toolset_get)(uint32_t dev_no);
    uint32_t (*video_resolution_get)(uint32_t dev_no);
    uint32_t (*min_frame_interval_get)(uint32_t dev_no);
    uint32_t (*slice_mode_get)(uint32_t dev_no);
    uint8_t (*rate_ctrl_mode_get)(uint32_t dev_no);
    uint32_t (*average_bitrate_get)(uint32_t dev_no);
    uint32_t (*cpb_size_get)(uint32_t dev_no);
    uint32_t (*peak_bit_rate_get)(uint32_t dev_no);
    uint64_t (*quantization_params_get)(uint32_t dev_no);
    uint32_t (*sync_ref_frame_get)(uint32_t dev_no);
    uint16_t (*ltr_buffer_get)(uint32_t dev_no);
    uint16_t (*ltr_picture_get)(uint32_t dev_no);
    uint16_t (*ltr_validation_get)(uint32_t dev_no);
    uint8_t (*level_idc_limit_get)(uint32_t dev_no);
    uint64_t (*sei_payloadtype_get)(uint32_t dev_no);
    uint16_t (*qp_range_get)(uint32_t dev_no);
    uint8_t (*priority_get)(uint32_t dev_no);
    uint8_t (*start_or_stop_layer_get)(uint32_t dev_no);
    uint16_t (*error_resiliency_get)(uint32_t dev_no);

    void (*select_layer_set)(uint32_t dev_no, uint16_t v);
    void (*profile_toolset_set)(uint32_t dev_no, uint16_t profile, uint16_t constrained_toolset, uint16_t settings);
    void (*video_resolution_set)(uint32_t dev_no, uint16_t width, uint16_t height);
    void (*min_frame_interval_set)(uint32_t dev_no, uint32_t v);
    void (*slice_mode_set)(uint32_t dev_no, uint16_t slice_mode, uint16_t slice_config);
    void (*rate_ctrl_mode_set)(uint32_t dev_no, uint16_t v);
    void (*average_bitrate_set)(uint32_t dev_no, uint32_t v);
    void (*cpb_size_set)(uint32_t dev_no, uint32_t v);
    void (*peak_bit_rate_set)(uint32_t dev_no, uint32_t v);
    void (*quantization_params_set)(uint32_t dev_no, uint16_t qp_prime_i, uint16_t qp_prime_p, uint16_t qp_prime_b);
    void (*sync_ref_frame_set)(uint32_t dev_no, uint8_t sync_frame_type, uint8_t sync_frame_interval,
        uint8_t gradual_decode_refresh);
    void (*ltr_buffer_set)(uint32_t dev_no, uint8_t roll_relative, uint8_t speed);
    void (*ltr_picture_set)(uint32_t dev_no, uint8_t put_at_pos_in_ltr_buffer, uint8_t ltr_mode);
    void (*ltr_validation_set)(uint32_t dev_no, uint16_t v);
    void (*level_idc_limit_set)(uint32_t dev_no, uint8_t v);
    void (*sei_payloadtype_set)(uint32_t dev_no, uint64_t v);
    void (*qp_range_set)(uint32_t dev_no, uint8_t min_qp, uint8_t max_qp);
    void (*priority_set)(uint32_t dev_no, uint8_t v);
    void (*start_or_stop_layer_set)(uint32_t dev_no, uint8_t v);
    void (*error_resiliency_set)(uint32_t dev_no, uint16_t v);
} encoding_units_ops_st;

#endif // __STREAM_EU_H__
