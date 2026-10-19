/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "sample_venc.h"
#include <stdio.h>
#include "sample_comm.h"
#include "ot_stream.h"
#include "uvc_media.h"

/* Stream Control Operation Functions End */
static struct processing_unit_ops g_venc_pu_ops = {
    .backlight_compensation_get = venc_backlight_compensation_get,
    .brightness_get = venc_brightness_get,
    .contrast_get = venc_contrast_get,
    .gain_get = venc_gain_get,
    .power_line_frequency_get = venc_power_line_frequency_get,
    .hue_get = venc_hue_get,
    .saturation_get = venc_saturation_get,
    .sharpness_get = venc_sharpness_get,
    .gamma_get = venc_gamma_get,
    .white_balance_temperature_get = venc_white_balance_temperature_get,
    .white_balance_temperature_auto_get = venc_white_balance_temperature_auto_get,
    .white_balance_component_get = venc_white_balance_component_get,
    .white_balance_component_auto_get = venc_white_balance_component_auto_get,
    .digital_multiplier_get = venc_digital_multiplier_get,
    .digital_multiplier_limit_get = venc_digital_multiplier_limit_get,
    .hue_auto_get = venc_hue_auto_get,
    .analog_video_standard_get = venc_analog_video_standard_get,
    .analog_lock_status_get = venc_analog_lock_status_get,
    .contrast_auto_get = venc_contrast_auto_get,

    .backlight_compensation_set = venc_backlight_compensation_set,
    .brightness_set = venc_brightness_set,
    .contrast_set = venc_contrast_set,
    .gain_set = venc_gain_set,
    .power_line_frequency_set = venc_power_line_frequency_set,
    .hue_set = venc_hue_set,
    .saturation_set = venc_saturation_set,
    .sharpness_set = venc_sharpness_set,
    .gamma_set = venc_gamma_set,
    .white_balance_temperature_set = venc_white_balance_temperature_set,
    .white_balance_temperature_auto_set = venc_white_balance_temperature_auto_set,
    .white_balance_component_set = venc_white_balance_component_set,
    .white_balance_component_auto_set = venc_white_balance_component_auto_set,
    .digital_multiplier_set = venc_digital_multiplier_set,
    .digital_multiplier_limit_set = venc_digital_multiplier_limit_set,
    .hue_auto_set = venc_hue_auto_set,
    .contrast_auto_set = venc_contrast_auto_set,
};

static struct camera_terminal_ops g_venc_ct_ops = {
    .exposure_ansolute_time_get = venc_exposure_ansolute_time_get,
    .exposure_auto_mode_get = venc_exposure_auto_mode_get,
    .scanning_mode_get = venc_scanning_mode_get,
    .exposure_auto_priority_get = venc_exposure_auto_priority_get,
    .exposure_relative_time_get = venc_exposure_relative_time_get,
    .focus_absolute_get = venc_focus_absolute_get,
    .focus_relative_get = venc_focus_relative_get,
    .focus_sample_get = venc_focus_sample_get,
    .focus_auto_get = venc_focus_auto_get,
    .iris_absolute_get = venc_iris_absolute_get,
    .iris_relative_get = venc_iris_relative_get,
    .zoom_absolute_get = venc_zoom_absolute_get,
    .zoom_relative_get = venc_zoom_relative_get,
    .pantilt_absolute_get = venc_pantilt_absolute_get,
    .pantilt_relative_get = venc_pantilt_relative_get,
    .roll_absolute_get = venc_roll_absolute_get,
    .roll_relative_get = venc_roll_relative_get,
    .ct_privacy_get = venc_ct_privacy_get,
    .ct_window_get = venc_ct_window_get,
    .region_of_interest_get = venc_region_of_interest_get,

    .exposure_ansolute_time_set = venc_exposure_ansolute_time_set,
    .exposure_auto_mode_set = venc_exposure_auto_mode_set,
    .scanning_mode_set = venc_scanning_mode_set,
    .exposure_auto_priority_set = venc_exposure_auto_priority_set,
    .exposure_relative_time_set = venc_exposure_relative_time_set,
    .focus_absolute_set = venc_focus_absolute_set,
    .focus_relative_set = venc_focus_relative_set,
    .focus_sample_set = venc_focus_sample_set,
    .focus_auto_set = venc_focus_auto_set,
    .iris_absolute_set = venc_iris_absolute_set,
    .iris_relative_set = venc_iris_relative_set,
    .zoom_absolute_set = venc_zoom_absolute_set,
    .zoom_relative_set = venc_zoom_relative_set,
    .pantilt_absolute_set = venc_pantilt_absolute_set,
    .pantilt_relative_set = venc_pantilt_relative_set,
    .roll_absolute_set = venc_roll_absolute_set,
    .roll_relative_set = venc_roll_relative_set,
    .ct_privacy_set = venc_ct_privacy_set,
    .ct_window_set = venc_ct_window_set,
    .region_of_interest_set = venc_region_of_interest_set,
};

static struct encoding_unit_ops g_venc_eu_ops = {
    .select_layer_get = venc_select_layer_get,
    .profile_toolset_get = venc_profile_toolset_get,
    .video_resolution_get = venc_video_resolution_get,
    .min_frame_interval_get = venc_min_frame_interval_get,
    .slice_mode_get = venc_slice_mode_get,
    .rate_ctrl_mode_get = venc_rate_ctrl_mode_get,
    .average_bitrate_get = venc_average_bitrate_get,
    .cpb_size_get = venc_cpb_size_get,
    .peak_bit_rate_get = venc_peak_bit_rate_get,
    .quantization_params_get = venc_quantization_params_get,
    .sync_ref_frame_get = venc_sync_ref_frame_get,
    .ltr_buffer_get = venc_ltr_buffer_get,
    .ltr_picture_get = venc_ltr_picture_get,
    .ltr_validation_get = venc_ltr_validation_get,
    .level_idc_limit_get = venc_level_idc_limit_get,
    .sei_payloadtype_get = venc_sei_payloadtype_get,
    .qp_range_get = venc_qp_range_get,
    .priority_get = venc_priority_get,
    .start_or_stop_layer_get = venc_start_or_stop_layer_get,
    .error_resiliency_get = venc_error_resiliency_get,

    .select_layer_set = venc_select_layer_set,
    .profile_toolset_set = venc_profile_toolset_set,
    .video_resolution_set = venc_video_resolution_set,
    .min_frame_interval_set = venc_min_frame_interval_set,
    .slice_mode_set = venc_slice_mode_set,
    .rate_ctrl_mode_set = venc_rate_ctrl_mode_set,
    .average_bitrate_set = venc_average_bitrate_set,
    .cpb_size_set = venc_cpb_size_set,
    .peak_bit_rate_set = venc_peak_bit_rate_set,
    .quantization_params_set = venc_quantization_params_set,
    .sync_ref_frame_set = venc_sync_ref_frame_set,
    .ltr_buffer_set = venc_ltr_buffer_set,
    .ltr_picture_set = venc_ltr_picture_set,
    .ltr_validation_set = venc_ltr_validation_set,
    .level_idc_limit_set = venc_level_idc_limit_set,
    .sei_payloadtype_set = venc_sei_payloadtype_set,
    .qp_range_set = venc_qp_range_set,
    .priority_set = venc_priority_set,
    .start_or_stop_layer_set = venc_start_or_stop_layer_set,
    .error_resiliency_set = venc_error_resiliency_set
};

static struct stream_control_ops g_venc_sc_ops = {
    .init = sample_venc_init,
    .deinit = sample_venc_deinit,
    .startup = sample_venc_startup,
    .shutdown = sample_venc_shutdown,
    .set_idr = sample_venc_set_idr,
    .set_property = sample_venc_set_property,
    .get_send = sample_venc_get_uvc_send,
};

td_void sample_venc_config(td_void)
{
    printf("\n@@@@@ UVC App Sample @@@@@\n\n");

    ot_stream_register_mpi_ops(&g_venc_sc_ops, &g_venc_pu_ops, &g_venc_ct_ops, &g_venc_eu_ops, NULL);
}
