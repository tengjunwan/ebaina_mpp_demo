/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __SAMPLE_UVC_MEDIA_H__
#define __SAMPLE_UVC_MEDIA_H__

#include "sample_comm.h"
#include "ot_common_uvc.h"
#include "ot_camera.h"
#include "sns_type.h"

typedef struct {
    sample_sns_type            sns_type;
    ot_size                    sensor_size;

    ot_vpss_grp_attr           vpss_grp_attr;
    td_bool                    vpss_chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM];
    ot_vpss_chn_attr           vpss_chn_attr[OT_VPSS_MAX_PHYS_CHN_NUM];
    sample_comm_venc_chn_param venc_chn_param;

    ot_vi_pipe      vi_pipe;
    ot_vi_chn       vi_chn;
    sample_vi_cfg   vi_cfg;
    ot_vpss_grp     vpss_grp;
    ot_vpss_chn     vpss_chn;
    ot_vpss_chn     vpss_ext_chn;
    ot_venc_chn     venc_chn;
    td_s32          venc_chn_num;
    td_u32          dev_no;

    td_u32          supplement_cfg;
} uvc_media_cfg;

#ifdef __cplusplus
extern "C" {
#endif

ot_uvc_data_input_mode sample_uvc_get_input_mode(td_u32 dev_no);
td_s32 sample_uvc_set_input_mode(td_u32 dev_no, ot_uvc_data_input_mode mode);

uint16_t venc_backlight_compensation_get(td_u32 dev_no);
uint16_t venc_brightness_get(td_u32 dev_no);
uint16_t venc_contrast_get(td_u32 dev_no);
uint16_t venc_gain_get(td_u32 dev_no);
uint16_t venc_hue_get(td_u32 dev_no);
uint8_t venc_power_line_frequency_get(td_u32 dev_no);
uint16_t venc_saturation_get(td_u32 dev_no);
uint16_t venc_sharpness_get(td_u32 dev_no);
uint16_t venc_gamma_get(td_u32 dev_no);
uint8_t venc_white_balance_temperature_auto_get(td_u32 dev_no);
uint16_t venc_white_balance_temperature_get(td_u32 dev_no);
uint32_t venc_white_balance_component_get(td_u32 dev_no);
uint8_t venc_white_balance_component_auto_get(td_u32 dev_no);
uint16_t venc_digital_multiplier_get(td_u32 dev_no);
uint16_t venc_digital_multiplier_limit_get(td_u32 dev_no);
uint8_t venc_hue_auto_get(td_u32 dev_no);
uint8_t venc_analog_video_standard_get(td_u32 dev_no);
uint8_t venc_analog_lock_status_get(td_u32 dev_no);
uint8_t venc_contrast_auto_get(td_u32 dev_no);
td_void venc_backlight_compensation_set(td_u32 dev_no, uint16_t v);
td_void venc_brightness_set(td_u32 dev_no, uint16_t v);
td_void venc_contrast_set(td_u32 dev_no, uint16_t v);
td_void venc_gain_set(td_u32 dev_no, uint16_t v);
td_void venc_hue_set(td_u32 dev_no, uint16_t v);
td_void venc_power_line_frequency_set(td_u32 dev_no, uint8_t v);
td_void venc_saturation_set(td_u32 dev_no, uint16_t v);
td_void venc_sharpness_set(td_u32 dev_no, uint16_t v);
td_void venc_gamma_set(td_u32 dev_no, uint16_t v);
td_void venc_white_balance_temperature_auto_set(td_u32 dev_no, uint8_t v);
td_void venc_white_balance_temperature_set(td_u32 dev_no, uint16_t v);
td_void venc_white_balance_component_set(td_u32 dev_no, uint32_t v);
td_void venc_white_balance_component_auto_set(td_u32 dev_no, uint8_t v);
td_void venc_digital_multiplier_set(td_u32 dev_no, uint16_t v);
td_void venc_digital_multiplier_limit_set(td_u32 dev_no, uint16_t v);
td_void venc_hue_auto_set(td_u32 dev_no, uint8_t v);
td_void venc_contrast_auto_set(td_u32 dev_no, uint8_t v);
uint8_t venc_exposure_auto_mode_get(td_u32 dev_no);
uint32_t venc_exposure_ansolute_time_get(td_u32 dev_no);
uint8_t venc_scanning_mode_get(td_u32 dev_no);
uint8_t venc_exposure_auto_priority_get(td_u32 dev_no);
uint8_t venc_exposure_relative_time_get(td_u32 dev_no);
uint16_t venc_focus_absolute_get(td_u32 dev_no);
uint16_t venc_focus_relative_get(td_u32 dev_no);
uint8_t venc_focus_sample_get(td_u32 dev_no);
uint8_t venc_focus_auto_get(td_u32 dev_no);
uint16_t venc_iris_absolute_get(td_u32 dev_no);
uint8_t venc_iris_relative_get(td_u32 dev_no);
uint16_t venc_zoom_absolute_get(td_u32 dev_no);
uint32_t venc_zoom_relative_get(td_u32 dev_no);
uint64_t venc_pantilt_absolute_get(td_u32 dev_no);
uint32_t venc_pantilt_relative_get(td_u32 dev_no);
uint16_t venc_roll_absolute_get(td_u32 dev_no);
uint16_t venc_roll_relative_get(td_u32 dev_no);
uint8_t venc_ct_privacy_get(td_u32 dev_no);
td_void venc_ct_window_get(td_u32 dev_no, ct_window_params *params);
td_void venc_region_of_interest_get(td_u32 dev_no, region_of_interest_params *params);
td_void venc_exposure_auto_mode_set(td_u32 dev_no, uint8_t v);
td_void venc_exposure_ansolute_time_set(td_u32 dev_no, uint32_t v);
td_void venc_scanning_mode_set(td_u32 dev_no, uint8_t v);
td_void venc_exposure_auto_priority_set(td_u32 dev_no, uint8_t v);
td_void venc_exposure_relative_time_set(td_u32 dev_no, uint8_t v);
td_void venc_focus_absolute_set(td_u32 dev_no, uint16_t v);
td_void venc_focus_relative_set(td_u32 dev_no, uint8_t focus_relative, uint8_t speed);
td_void venc_focus_sample_set(td_u32 dev_no, uint8_t v);
td_void venc_focus_auto_set(td_u32 dev_no, uint8_t v);
td_void venc_iris_absolute_set(td_u32 dev_no, uint16_t v);
td_void venc_iris_relative_set(td_u32 dev_no, uint8_t v);
td_void venc_zoom_absolute_set(td_u32 dev_no, uint16_t v);
td_void venc_zoom_relative_set(td_u32 dev_no, uint16_t zoom, uint16_t digital_zoom, uint16_t speed);
td_void venc_pantilt_absolute_set(td_u32 dev_no, uint32_t pan_absolute, uint32_t tilt_absolute);
td_void venc_pantilt_relative_set(td_u32 dev_no, uint8_t pan_relative, uint8_t pan_speed, uint8_t tilt_relative,
    uint8_t tilt_speed);
td_void venc_roll_absolute_set(td_u32 dev_no, uint16_t v);
td_void venc_roll_relative_set(td_u32 dev_no, uint8_t roll_relative, uint8_t speed);
td_void venc_ct_privacy_set(td_u32 dev_no, uint8_t v);
td_void venc_ct_window_set(td_u32 dev_no, const ct_window_params *params);
td_void venc_region_of_interest_set(td_u32 dev_no, const region_of_interest_params *params);
uint16_t venc_select_layer_get(td_u32 dev_no);
uint64_t venc_profile_toolset_get(td_u32 dev_no);
uint32_t venc_video_resolution_get(td_u32 dev_no);
uint32_t venc_min_frame_interval_get(td_u32 dev_no);
uint32_t venc_slice_mode_get(td_u32 dev_no);
uint8_t venc_rate_ctrl_mode_get(td_u32 dev_no);
uint32_t venc_average_bitrate_get(td_u32 dev_no);
uint32_t venc_cpb_size_get(td_u32 dev_no);
uint32_t venc_peak_bit_rate_get(td_u32 dev_no);
uint64_t venc_quantization_params_get(td_u32 dev_no);
uint32_t venc_sync_ref_frame_get(td_u32 dev_no);
uint16_t venc_ltr_buffer_get(td_u32 dev_no);
uint16_t venc_ltr_picture_get(td_u32 dev_no);
uint16_t venc_ltr_validation_get(td_u32 dev_no);
uint8_t venc_level_idc_limit_get(td_u32 dev_no);
uint64_t venc_sei_payloadtype_get(td_u32 dev_no);
uint16_t venc_qp_range_get(td_u32 dev_no);
uint8_t venc_priority_get(td_u32 dev_no);
uint8_t venc_start_or_stop_layer_get(td_u32 dev_no);
uint16_t venc_error_resiliency_get(td_u32 dev_no);
td_void venc_select_layer_set(td_u32 dev_no, uint16_t v);
td_void venc_profile_toolset_set(td_u32 dev_no, uint16_t profile, uint16_t constrained_toolset, uint16_t settings);
td_void venc_video_resolution_set(td_u32 dev_no, uint16_t width, uint16_t height);
td_void venc_min_frame_interval_set(td_u32 dev_no, uint32_t v);
td_void venc_slice_mode_set(td_u32 dev_no, uint16_t slice_mode, uint16_t slice_config);
td_void venc_rate_ctrl_mode_set(td_u32 dev_no, uint16_t v);
td_void venc_average_bitrate_set(td_u32 dev_no, uint32_t v);
td_void venc_cpb_size_set(td_u32 dev_no, uint32_t v);
td_void venc_peak_bit_rate_set(td_u32 dev_no, uint32_t v);
td_void venc_quantization_params_set(td_u32 dev_no, uint16_t qp_prime_i, uint16_t qp_prime_p, uint16_t qp_prime_b);
td_void venc_sync_ref_frame_set(td_u32 dev_no, uint8_t sync_frame_type, uint8_t sync_frame_interval,
    uint8_t gradual_decode_refresh);
td_void venc_ltr_buffer_set(td_u32 dev_no, uint8_t ltr_buffer_size, uint8_t ltr_encoder_control);
td_void venc_ltr_picture_set(td_u32 dev_no, uint8_t put_at_pos_in_ltr_buffer, uint8_t ltr_mode);
td_void venc_ltr_validation_set(td_u32 dev_no, uint16_t v);
td_void venc_level_idc_limit_set(td_u32 dev_no, uint8_t v);
td_void venc_sei_payloadtype_set(td_u32 dev_no, uint64_t v);
td_void venc_qp_range_set(td_u32 dev_no, uint8_t min_qp, uint8_t max_qp);
td_void venc_priority_set(td_u32 dev_no, uint8_t v);
td_void venc_start_or_stop_layer_set(td_u32 dev_no, uint8_t v);
td_void venc_error_resiliency_set(td_u32 dev_no, uint16_t v);

td_s32 sample_venc_set_idr(td_u32 dev_no);
#ifdef UVC_STILL_IMAGE
td_s32 sample_get_still_image(struct video_framebuffer *still_image);
#endif
td_s32 sample_venc_init(td_void);
td_s32 sample_venc_deinit(td_void);
td_s32 sample_venc_startup(td_u32 dev_no);
td_s32 sample_venc_shutdown(td_u32 dev_no);

td_s32 sample_venc_set_property(td_u32 dev_no, encoder_property *p);
td_void sample_uvc_get_encoder_property(td_u32 dev_no, encoder_property *p);
td_s32 sample_venc_get_uvc_send(td_u32 dev_no);

#ifdef __cplusplus
}
#endif

#endif /* __SAMPLE_UVC_MEDIA_H__ */
