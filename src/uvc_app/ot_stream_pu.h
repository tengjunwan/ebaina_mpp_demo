/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __STREAM_PU_H__
#define __STREAM_PU_H__

#include <stdint.h>
#include "uvc.h"

#define OT_UVC_PU_CONTROL_UNDEFINED 0x00
#define OT_UVC_PU_BACKLIGHT_COMPENSATION_CONTROL 0x01
#define OT_UVC_PU_BRIGHTNESS_CONTROL 0x02
#define OT_UVC_PU_CONTRAST_CONTROL 0x03
#define OT_UVC_PU_GAIN_CONTROL 0x04
#define OT_UVC_PU_POWER_LINE_FREQUENCY_CONTROL 0x05
#define OT_UVC_PU_HUE_CONTROL 0x06
#define OT_UVC_PU_SATURATION_CONTROL 0x07
#define OT_UVC_PU_SHARPNESS_CONTROL 0x08
#define OT_UVC_PU_GAMMA_CONTROL 0x09
#define OT_UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL 0x0a
#define OT_UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL 0x0b
#define OT_UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL 0x0c
#define OT_UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL 0x0d
#define OT_UVC_PU_DIGITAL_MULTIPLIER_CONTROL 0x0e
#define OT_UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL 0x0f
#define OT_UVC_PU_HUE_AUTO_CONTROL 0x10
#define OT_UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL 0x11
#define OT_UVC_PU_ANALOG_LOCK_STATUS_CONTROL 0x12
#define OT_UVC_PU_CONTRAST_AUTO_CONTROL		0x13

typedef struct processing_unit_ops {
    uint16_t (*backlight_compensation_get)(uint32_t dev_no);
    uint16_t (*brightness_get)(uint32_t dev_no);
    uint16_t (*contrast_get)(uint32_t dev_no);
    uint16_t (*gain_get)(uint32_t dev_no);
    uint8_t (*power_line_frequency_get)(uint32_t dev_no);
    uint16_t (*hue_get)(uint32_t dev_no);
    uint16_t (*saturation_get)(uint32_t dev_no);
    uint16_t (*sharpness_get)(uint32_t dev_no);
    uint16_t (*gamma_get)(uint32_t dev_no);
    uint16_t (*white_balance_temperature_get)(uint32_t dev_no);
    uint8_t (*white_balance_temperature_auto_get)(uint32_t dev_no);
    uint32_t (*white_balance_component_get)(uint32_t dev_no);
    uint8_t (*white_balance_component_auto_get)(uint32_t dev_no);
    uint16_t (*digital_multiplier_get)(uint32_t dev_no);
    uint16_t (*digital_multiplier_limit_get)(uint32_t dev_no);
    uint8_t (*hue_auto_get)(uint32_t dev_no);
    uint8_t (*analog_video_standard_get)(uint32_t dev_no);
    uint8_t (*analog_lock_status_get)(uint32_t dev_no);
    uint8_t (*contrast_auto_get)(uint32_t dev_no);

    void (*backlight_compensation_set)(uint32_t dev_no, uint16_t v);
    void (*brightness_set)(uint32_t dev_no, uint16_t v);
    void (*contrast_set)(uint32_t dev_no, uint16_t v);
    void (*gain_set)(uint32_t dev_no, uint16_t v);
    void (*power_line_frequency_set)(uint32_t dev_no, uint8_t v);
    void (*hue_set)(uint32_t dev_no, uint16_t v);
    void (*saturation_set)(uint32_t dev_no, uint16_t v);
    void (*sharpness_set)(uint32_t dev_no, uint16_t v);
    void (*gamma_set)(uint32_t dev_no, uint16_t v);
    void (*white_balance_temperature_set)(uint32_t dev_no, uint16_t v);
    void (*white_balance_temperature_auto_set)(uint32_t dev_no, uint8_t v);
    void (*white_balance_component_set)(uint32_t dev_no, uint32_t v);
    void (*white_balance_component_auto_set)(uint32_t dev_no, uint8_t v);
    void (*digital_multiplier_set)(uint32_t dev_no, uint16_t v);
    void (*digital_multiplier_limit_set)(uint32_t dev_no, uint16_t v);
    void (*hue_auto_set)(uint32_t dev_no, uint8_t v);
    void (*contrast_auto_set)(uint32_t dev_no, uint8_t v);
} processing_unit_ops_st;

#endif // __STREAM_PU_H__
