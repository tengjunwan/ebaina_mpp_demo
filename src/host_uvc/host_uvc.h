/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef HOST_UVC_H
#define HOST_UVC_H

#include <linux/videodev2.h>
#include "sample_comm.h"

#ifndef V4L2_PIX_FMT_H265
#define V4L2_PIX_FMT_H265     v4l2_fourcc('H', '2', '6', '5') /* H.265 aka HEVC */
#endif

enum buffer_fill_mode {
    BUFFER_FILL_NONE = 0,
    BUFFER_FILL_FRAME = 1 << 0,
    BUFFER_FILL_PADDING = 1 << 1,
};

typedef struct {
    td_u32 idx;
    td_u32 padding[VIDEO_MAX_PLANES];
    td_u32 size[VIDEO_MAX_PLANES];
    td_void *mem[VIDEO_MAX_PLANES];
} buffer_info;

typedef struct {
    td_s32 fd;
    td_bool opened;
    enum v4l2_buf_type type;
    enum v4l2_memory memtype;
    td_u32 nbufs;
    buffer_info *buffers;
    td_u32 width;
    td_u32 height;
    uint32_t buffer_output_flags;
    uint32_t timestamp_type;
    td_u32 num_planes;
    struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];
    td_void *pattern[VIDEO_MAX_PLANES];
    td_u32 patternsize[VIDEO_MAX_PLANES];
    td_bool write_data_prefix;
} device_info;

typedef struct {
    const td_char *name;
    td_u32 fourcc;
    td_u8 n_planes;
} format_info;

typedef struct {
    const td_char *name;
    enum v4l2_field field;
} field_info;

typedef struct {
    td_u32 nframes;
    td_u32 skip;
    td_u32 delay;
    td_bool do_requeue_last;
    td_bool do_queue_late;
    enum buffer_fill_mode fill;
    const td_char *pattern;
    const td_char *type_name;

    td_bool do_capture;
    td_bool do_set_format;
    td_u32 pixelformat;
    td_bool do_file;
    td_u32 nbufs;
    td_bool do_rt;
    td_s32 rt_priority;
    td_u32 width;
    td_u32 height;
    td_u32 stride;
    td_u32 buffer_size;
    td_bool do_set_time_per_frame;
    struct v4l2_fract time_per_frame;
    enum v4l2_memory memtype;
    const td_char *ctrl_value;
    td_bool do_enum_formats;
    enum v4l2_field field;
    td_bool no_query;
    td_u32 fmt_flags;
    td_u32 userptr_offset;
} uvc_ctrl_info;

#endif
