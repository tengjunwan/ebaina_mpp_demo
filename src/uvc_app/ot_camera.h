/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef OT_CAMERA_H
#define OT_CAMERA_H

typedef enum ot_stream_type_e {
    OT_FORMAT_H264 = 0x1,
    OT_FORMAT_MJPEG = 0x2,
    OT_FORMAT_MJPG = 0x2,
    OT_FORMAT_YUV = 0x3,
    OT_FORMAT_YUV420 = 0x3
} ot_stream_type_e;

typedef enum ot_stream_resolution_e {
    OT_RESOLUTION_1080 = 0x1,
    OT_RESOLUTION_720 = 0x2,
    OT_RESOLUTION_360 = 0x3
} ot_stream_resolution_e;

typedef struct encoder_property {
    unsigned int format;
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    unsigned char compsite;
} encoder_property;

typedef struct {
    unsigned short window_top;
    unsigned short window_left;
    unsigned short window_bottom;
    unsigned short window_right;
    unsigned short num_steps;
    unsigned short num_step_units;
} ct_window_params;

typedef struct {
    unsigned short roi_top;
    unsigned short roi_left;
    unsigned short roi_bottom;
    unsigned short roi_right;
    unsigned short auto_controls;
} region_of_interest_params;

#endif
