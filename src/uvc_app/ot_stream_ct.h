/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __STREAM_CT_H__
#define __STREAM_CT_H__

#include <stdint.h>
#include "ot_camera.h"
#include "uvc.h"

#define OT_UVC_CT_CONTROL_UNDEFINED 0x00
#define OT_UVC_CT_SCANNING_MODE_CONTROL 0x01
#define OT_UVC_CT_AE_MODE_CONTROL 0x02
#define OT_UVC_CT_AE_PRIORITY_CONTROL 0x03
#define OT_UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL 0x04
#define OT_UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL 0x05
#define OT_UVC_CT_FOCUS_ABSOLUTE_CONTROL 0x06
#define OT_UVC_CT_FOCUS_RELATIVE_CONTROL 0x07
#define OT_UVC_CT_FOCUS_AUTO_CONTROL 0x08
#define OT_UVC_CT_IRIS_ABSOLUTE_CONTROL 0x09
#define OT_UVC_CT_IRIS_RELATIVE_CONTROL 0x0a
#define OT_UVC_CT_ZOOM_ABSOLUTE_CONTROL 0x0b
#define OT_UVC_CT_ZOOM_RELATIVE_CONTROL 0x0c
#define OT_UVC_CT_PANTILT_ABSOLUTE_CONTROL 0x0d
#define OT_UVC_CT_PANTILT_RELATIVE_CONTROL 0x0e
#define OT_UVC_CT_ROLL_ABSOLUTE_CONTROL 0x0f
#define OT_UVC_CT_ROLL_RELATIVE_CONTROL 0x10
#define OT_UVC_CT_PRIVACY_CONTROL 0x11
#define OT_UVC_CT_FOCUS_SIMPLE_CONTROL 0x12
#define OT_UVC_CT_WINDOW_CONTROL 0x13
#define OT_UVC_CT_REGION_OF_INTEREST_CONTROL 0x14

typedef struct camera_terminal_ops {
    uint32_t (*exposure_ansolute_time_get)(uint32_t dev_no);
    uint8_t (*exposure_auto_mode_get)(uint32_t dev_no);
    uint8_t (*scanning_mode_get)(uint32_t dev_no);
    uint8_t (*exposure_auto_priority_get)(uint32_t dev_no);
    uint8_t (*exposure_relative_time_get)(uint32_t dev_no);
    uint16_t (*focus_absolute_get)(uint32_t dev_no);
    uint16_t (*focus_relative_get)(uint32_t dev_no);
    uint8_t (*focus_sample_get)(uint32_t dev_no);
    uint8_t (*focus_auto_get)(uint32_t dev_no);
    uint16_t (*iris_absolute_get)(uint32_t dev_no);
    uint8_t (*iris_relative_get)(uint32_t dev_no);
    uint16_t (*zoom_absolute_get)(uint32_t dev_no);
    uint32_t (*zoom_relative_get)(uint32_t dev_no);
    uint64_t (*pantilt_absolute_get)(uint32_t dev_no);
    uint32_t (*pantilt_relative_get)(uint32_t dev_no);
    uint16_t (*roll_absolute_get)(uint32_t dev_no);
    uint16_t (*roll_relative_get)(uint32_t dev_no);
    uint8_t (*ct_privacy_get)(uint32_t dev_no);
    void (*ct_window_get)(uint32_t dev_no, ct_window_params *params);
    void (*region_of_interest_get)(uint32_t dev_no, region_of_interest_params *params);

    void (*exposure_ansolute_time_set)(uint32_t dev_no, uint32_t v);
    void (*exposure_auto_mode_set)(uint32_t dev_no, uint8_t v);
    void (*scanning_mode_set)(uint32_t dev_no, uint8_t v);
    void (*exposure_auto_priority_set)(uint32_t dev_no, uint8_t v);
    void (*exposure_relative_time_set)(uint32_t dev_no, uint8_t v);
    void (*focus_absolute_set)(uint32_t dev_no, uint16_t v);
    void (*focus_relative_set)(uint32_t dev_no, uint8_t focus_relative, uint8_t speed);
    void (*focus_sample_set)(uint32_t dev_no, uint8_t v);
    void (*focus_auto_set)(uint32_t dev_no, uint8_t v);
    void (*iris_absolute_set)(uint32_t dev_no, uint16_t v);
    void (*iris_relative_set)(uint32_t dev_no, uint8_t v);
    void (*zoom_absolute_set)(uint32_t dev_no, uint16_t v);
    void (*zoom_relative_set)(uint32_t dev_no, uint16_t zoom, uint16_t digital_zoom, uint16_t speed);
    void (*pantilt_absolute_set)(uint32_t dev_no, uint32_t pan_absolute, uint32_t tilt_absolute);
    void (*pantilt_relative_set)(uint32_t dev_no, uint8_t pan_relative, uint8_t pan_speed, uint8_t tilt_relative,
        uint8_t tilt_speed);
    void (*roll_absolute_set)(uint32_t dev_no, uint16_t v);
    void (*roll_relative_set)(uint32_t dev_no, uint8_t roll_relative, uint8_t speed);
    void (*ct_privacy_set)(uint32_t dev_no, uint8_t v);
    void (*ct_window_set)(uint32_t dev_no, const ct_window_params *params);
    void (*region_of_interest_set)(uint32_t dev_no, const region_of_interest_params *params);
} camera_terminal_ops_st;

#endif // __STREAM_CT_H__
