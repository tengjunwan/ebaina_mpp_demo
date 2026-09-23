/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "host_uvc.h"
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>
#include "media_vdec.h"


#define array_size(a)    (sizeof(a) / sizeof((a)[0]))

#define SAMPLE_UVC_V4L_BUFFERS_DEFAULT  1
#define SAMPLE_UVC_V4L_BUFFERS_MAX      4

#define SAMPLE_UVC_OPT_ENUM_FORMATS   256

#define SAMPLE_UVC_MAX_FILE_NAME_LEN  23

#define DEVICE_UVC_SIZE_NUM           20

ot_size g_device_size[DEVICE_UVC_SIZE_NUM] = {0};
td_u32 g_device_size_num = 0;

td_bool g_uvc_exit = TD_FALSE;

static format_info g_pixel_formats[] = {
    { "YUYV", V4L2_PIX_FMT_YUYV, 1 },
    { "MJPEG", V4L2_PIX_FMT_MJPEG, 1 },
    { "H264", V4L2_PIX_FMT_H264, 1 },
    { "H265", V4L2_PIX_FMT_H265, 1 },
    { "NV12", V4L2_PIX_FMT_NV12, 1 },
    { "NV21", V4L2_PIX_FMT_NV21, 1 },
};

static field_info g_fields[] = {
    { "filed_any", V4L2_FIELD_ANY },
    { "filed_none", V4L2_FIELD_NONE },
    { "filed_top", V4L2_FIELD_TOP },
    { "filed_bottom", V4L2_FIELD_BOTTOM },
    { "filed_interlaced", V4L2_FIELD_INTERLACED },
    { "filed_seq_tb", V4L2_FIELD_SEQ_TB },
    { "filed_seq_bt", V4L2_FIELD_SEQ_BT },
    { "filed_alternate", V4L2_FIELD_ALTERNATE },
    { "filed_interlaced_tb", V4L2_FIELD_INTERLACED_TB },
    { "filed_interlaced_bt", V4L2_FIELD_INTERLACED_BT },
};

static struct option g_opts[] = {
    {"enum-formats", 0, 0, SAMPLE_UVC_OPT_ENUM_FORMATS},
    {"file", 2, 0, 'F'},
    {"format", 1, 0, 'f'},
    {"help", 0, 0, 'h'},
    {"size", 1, 0, 's'},
    {0, 0, 0, 0}
};

static td_void sample_uvc_exit_signal_handler(td_s32 signal __attribute__((__unused__)))
{
    g_uvc_exit = TD_TRUE;
}

static td_bool sample_uvc_video_is_capture(const device_info *dev)
{
    return (dev->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || dev->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
        dev->type == V4L2_BUF_TYPE_META_CAPTURE);
}

static const td_char *sample_uvc_v4l2_buf_type_name(enum v4l2_buf_type type)
{
    switch (type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
            return "Video capture";
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
            return "Video capture mplanes";
        default:
            return "Other";
    }
}

static td_void sample_uvc_list_formats(td_void)
{
    td_u32 i;

    for (i = 0; i < array_size(g_pixel_formats); i++) {
        sample_print("%s (\"%u%u%u%u\", %u planes)\n", g_pixel_formats[i].name, g_pixel_formats[i].fourcc & 0xff,
            (g_pixel_formats[i].fourcc >> 8) & 0xff, (g_pixel_formats[i].fourcc >> 16) & 0xff,  /* 8,16:shift */
            (g_pixel_formats[i].fourcc >> 24) & 0xff, g_pixel_formats[i].n_planes);             /* 24:shift */
    }
}

static const format_info *sample_uvc_v4l2_format_by_fourcc(td_u32 fourcc)
{
    td_u32 i;

    for (i = 0; i < array_size(g_pixel_formats); ++i) {
        if (g_pixel_formats[i].fourcc == fourcc) {
            return &g_pixel_formats[i];
        }
    }

    return TD_NULL;
}

static const format_info *sample_uvc_v4l2_format_by_name(const td_char *name)
{
    td_u32 i;

    for (i = 0; i < array_size(g_pixel_formats); ++i) {
        if (strcasecmp(g_pixel_formats[i].name, name) == 0) {
            return &g_pixel_formats[i];
        }
    }

    return TD_NULL;
}

static const td_char *sample_uvc_v4l2_format_name(td_u32 fourcc)
{
    const format_info *format;
    static td_char format_name[5];     /* 5: array len */
    td_u32 i;
    td_u32 local_fourcc = fourcc;

    format = sample_uvc_v4l2_format_by_fourcc(fourcc);
    if (format) {
        return format->name;
    }

    for (i = 0; i < 4; ++i) {       /* 4: format */
        format_name[i] = local_fourcc & 0xff;
        local_fourcc >>= 8;               /* 8: shift */
    }

    format_name[4] = '\0';     /* 4: end */
    return format_name;
}

static const td_char *sample_uvc_v4l2_field_name(enum v4l2_field field)
{
    td_u32 i;

    for (i = 0; i < array_size(g_fields); ++i) {
        if (g_fields[i].field == field) {
            return g_fields[i].name;
        }
    }

    return "unknown";
}

static td_void sample_uvc_video_set_buf_type(device_info *dev, enum v4l2_buf_type type)
{
    dev->type = type;
}

static td_bool sample_uvc_video_has_valid_buf_type(const device_info *dev)
{
    return (td_s32)dev->type != -1;
}

static td_void sample_uvc_video_init(device_info *dev)
{
    dev->fd = -1;
    dev->memtype = V4L2_MEMORY_MMAP;
    dev->buffers = TD_NULL;
    dev->type = (enum v4l2_buf_type)-1;
}

static td_bool sample_uvc_video_has_fd(const device_info *dev)
{
    return dev->fd != -1;
}

static td_s32 sample_uvc_video_open(device_info *dev, const td_char *devname)
{
    if (sample_uvc_video_has_fd(dev)) {
        sample_print("Can't open device (already open).\n");
        return -1;
    }
    td_char *regular_path = realpath(devname, NULL);
    if (regular_path == NULL) {
        sample_print("Can't get regular path of %s\n", devname);
        return -1;
    }
    dev->fd = open(regular_path, O_RDWR);
    free(regular_path);
    if (dev->fd < 0) {
        sample_print("Error opening device %s\n", devname);
        return dev->fd;
    }

    printf("Device %s opened.\n", devname);

    dev->opened = TD_TRUE;

    return 0;
}

static td_s32 sample_uvc_video_query_cap(device_info *dev, td_u32 *capabilities)
{
    struct v4l2_capability cap;
    td_u32 caps;
    td_bool is_support_video;
    td_bool is_support_meta;
    td_bool is_support_capture;
    td_bool is_support_output;
    td_bool is_support_mplane;
    td_s32 ret;

    (td_void)memset_s(&cap, sizeof(cap), 0, sizeof(cap));
    ret = ioctl(dev->fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        return 0;
    }

    caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;

    is_support_video = caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_CAPTURE |
        V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_OUTPUT);
    is_support_meta = caps & (V4L2_CAP_META_CAPTURE);
    is_support_capture = caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_META_CAPTURE);
    is_support_output = caps & (V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_OUTPUT);
    is_support_mplane = caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE);

    printf("Device `%s' on `%s' (driver '%s') supports%s%s%s%s %s mplanes.\n", cap.card, cap.bus_info, cap.driver,
        is_support_video ? " video," : "", is_support_meta ? " meta-data," : "",
        is_support_capture ? " capture," : "", is_support_output ? " output," : "",
        is_support_mplane ? "with" : "without");

    *capabilities = caps;

    return 0;
}

static td_s32 sample_uvc_cap_get_buf_type(td_u32 capa)
{
    if (capa & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        return V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }

    if (capa & V4L2_CAP_VIDEO_OUTPUT_MPLANE) {
        return V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    }

    if (capa & V4L2_CAP_VIDEO_CAPTURE) {
        return  V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

    if (capa & V4L2_CAP_VIDEO_OUTPUT) {
        return V4L2_BUF_TYPE_VIDEO_OUTPUT;
    }

    if (capa & V4L2_CAP_META_CAPTURE) {
        return V4L2_BUF_TYPE_META_CAPTURE;
    }

    sample_print("Device supports neither capture nor output.\n");
    return -EINVAL;
}

static td_void sample_uvc_video_close(device_info *dev)
{
    td_u32 i;

    for (i = 0; i < dev->num_planes; i++) {
        if (dev->pattern[i] != TD_NULL) {
            free(dev->pattern[i]);
            dev->pattern[i] = TD_NULL;
        }
    }

    if (dev->buffers != TD_NULL) {
        free(dev->buffers);
        dev->buffers = TD_NULL;
    }

    if (dev->opened) {
        close(dev->fd);
        dev->fd = -1;
    }
}

static td_s32 sample_uvc_video_get_format(device_info *dev)
{
    struct v4l2_format format = {0};
    td_s32 ret;

    format.type = dev->type;

    ret = ioctl(dev->fd, VIDIOC_G_FMT, &format);
    if (ret < 0) {
        sample_print("get format failed!\n");
        return ret;
    }

    dev->width = format.fmt.pix.width;
    dev->height = format.fmt.pix.height;
    dev->num_planes = 1;

    dev->plane_fmt[0].bytesperline = format.fmt.pix.bytesperline;
    dev->plane_fmt[0].sizeimage = (format.fmt.pix.bytesperline == 0 ? format.fmt.pix.sizeimage : 0);

    printf("video format: %s (%08x) %ux%u (stride %u) field %s buffer size %u\n",
        sample_uvc_v4l2_format_name(format.fmt.pix.pixelformat), format.fmt.pix.pixelformat,
        format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline,
        sample_uvc_v4l2_field_name(format.fmt.pix_mp.field),
        format.fmt.pix.sizeimage);

    return 0;
}

static td_s32 sample_uvc_get_device_size(device_info *dev, td_u32 pixelformat, enum v4l2_buf_type type)
{
    struct v4l2_frmsizeenum frame;
    td_u32 i;
    td_s32 ret;

    for (i = 0; ; ++i) {
        (td_void)memset_s(&frame, sizeof(frame), 0, sizeof(frame));

        frame.index = i;
        frame.pixel_format = pixelformat;

        ret = ioctl(dev->fd, VIDIOC_ENUM_FRAMESIZES, &frame);
        if (ret < 0) {
            break;
        }

        g_device_size[i].width = frame.discrete.width;
        g_device_size[i].height = frame.discrete.height;
        g_device_size_num++;
    }

    if (i >= DEVICE_UVC_SIZE_NUM) {
        sample_print("DEVICE_UVC_SIZE_NUM(%d) is less than %u, change it's value by yourself!\n",
            DEVICE_UVC_SIZE_NUM, i);
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_s32 sample_uvc_check_video_size(device_info *dev, td_u32 pixelformat, td_u32 width, td_u32 height)
{
    td_u32 i;
    td_s32 ret;

    if (pixelformat == V4L2_PIX_FMT_NV21 || pixelformat == V4L2_PIX_FMT_NV12) {
        return TD_SUCCESS;
    }

    ret = sample_uvc_get_device_size(dev, pixelformat, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    for (i = 0; i < g_device_size_num; i++) {
        if ((width == g_device_size[i].width) && (height == g_device_size[i].height)) {
            return TD_SUCCESS;
        }
    }

    sample_print("usb device not support this video size(w:%u h:%u) or not support this format!\n", width, height);
    return TD_FAILURE;
}

static td_s32 sample_uvc_video_set_format(device_info *dev, uvc_ctrl_info *ctrl_info)
{
    struct v4l2_format format = {0};
    td_s32 ret;

    format.type = dev->type;
    format.fmt.pix.width = ctrl_info->width;
    format.fmt.pix.height = ctrl_info->height;
    format.fmt.pix.pixelformat = ctrl_info->pixelformat;
    format.fmt.pix.field = ctrl_info->field;
    format.fmt.pix.bytesperline = ctrl_info->stride;
    format.fmt.pix.sizeimage = ctrl_info->buffer_size;
    format.fmt.pix.priv = V4L2_PIX_FMT_PRIV_MAGIC;
    format.fmt.pix.flags = ctrl_info->fmt_flags;

    ret = sample_uvc_check_video_size(dev, ctrl_info->pixelformat, format.fmt.pix.width, format.fmt.pix.height);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    ret = ioctl(dev->fd, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        sample_print("set format failed!\n");
        return ret;
    }

    printf("video format set: %s (%08x) %ux%u (stride %u) field %s buffer size %u\n",
        sample_uvc_v4l2_format_name(format.fmt.pix.pixelformat), format.fmt.pix.pixelformat, format.fmt.pix.width,
        format.fmt.pix.height, format.fmt.pix.bytesperline, sample_uvc_v4l2_field_name(format.fmt.pix.field),
        format.fmt.pix.sizeimage);

    return 0;
}

static td_s32 sample_uvc_video_set_frame_rate(device_info *dev, struct v4l2_fract *time_per_frame)
{
    struct v4l2_streamparm stream_parm = {0};
    td_s32 ret;

    stream_parm.type = dev->type;

    ret = ioctl(dev->fd, VIDIOC_G_PARM, &stream_parm);
    if (ret < 0) {
        sample_print("get frame rate failed!\n");
        return ret;
    }

    printf("Current frame rate: %u/%u\n", stream_parm.parm.capture.timeperframe.numerator,
        stream_parm.parm.capture.timeperframe.denominator);

    printf("Setting frame rate to: %u/%u\n", time_per_frame->numerator,
        time_per_frame->denominator);

    stream_parm.parm.capture.timeperframe.numerator = time_per_frame->numerator;
    stream_parm.parm.capture.timeperframe.denominator = time_per_frame->denominator;

    ret = ioctl(dev->fd, VIDIOC_S_PARM, &stream_parm);
    if (ret < 0) {
        sample_print("Unable to set frame rate.\n");
        return ret;
    }

    ret = ioctl(dev->fd, VIDIOC_G_PARM, &stream_parm);
    if (ret < 0) {
        sample_print("Unable to get frame rate.\n");
        return ret;
    }

    printf("Frame rate set: %u/%u\n", stream_parm.parm.capture.timeperframe.numerator,
        stream_parm.parm.capture.timeperframe.denominator);
    return 0;
}

static td_s32 sample_uvc_video_buffer_mmap(device_info *dev, buffer_info *buffer, struct v4l2_buffer *v4l2buf)
{
    td_u32 length;
    td_u32 offset;
    td_u32 i;
    td_s32 ret;

    (td_void)memset_s(buffer->mem, sizeof(buffer->mem), 0, sizeof(buffer->mem));
    for (i = 0; i < dev->num_planes; i++) {
        length = v4l2buf->length;
        offset = v4l2buf->m.offset;

        buffer->mem[i] = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, offset);
        if (buffer->mem[i] == MAP_FAILED) {
            sample_print("Unable to map buffer %u/%u\n", buffer->idx, i);
            goto fail;
        }

        buffer->size[i] = length;
        buffer->padding[i] = 0;

        printf("Buffer %u/%u mapped\n", buffer->idx, i);
    }

    return 0;
fail:
    for (i = 0; i < dev->num_planes; i++) {
        if (buffer->mem[i] == TD_NULL) {
            continue;
        }

        length = v4l2buf->length;
        offset = v4l2buf->m.offset;
        ret = munmap(buffer->mem[i], length);
        if (ret != 0) {
            sample_print("munmap fail, errno %d\n", errno);
        }
        buffer->mem[i] = TD_NULL;
    }
    return -1;
}

static td_s32 sample_uvc_video_buffer_munmap(const device_info *dev, buffer_info *buffer)
{
    td_u32 i;
    td_s32 ret;

    for (i = 0; i < dev->num_planes; i++) {
        ret = munmap(buffer->mem[i], buffer->size[i]);
        if (ret < 0) {
            sample_print("Unable to unmap buffer %u/%u\n", buffer->idx, i);
        }

        buffer->mem[i] = TD_NULL;
    }

    return 0;
}

static td_s32 sample_uvc_video_buffer_alloc_userptr(const device_info *dev, buffer_info *buffer,
    struct v4l2_buffer *v4l2buf, td_u32 offset, td_u32 padding)
{
    td_s32 page_size = getpagesize();
    td_u32 length;
    td_u32 i;
    td_s32 ret;

    (td_void)memset_s(buffer->mem, sizeof(buffer->mem), 0, sizeof(buffer->mem));
    for (i = 0; i < dev->num_planes; i++) {
        length = v4l2buf->length;

        ret = posix_memalign(&buffer->mem[i], (size_t)page_size, (size_t)(length + offset + padding));
        if (ret != 0 || buffer->mem[i] == NULL) {
            sample_print("Unable to allocate buffer %u/%u (%d)\n", buffer->idx, i, ret);
            goto fail;
        }

        buffer->mem[i] += offset;
        buffer->size[i] = length;
        buffer->padding[i] = padding;

        printf("Buffer %u/%u allocated\n", buffer->idx, i);
    }

    return 0;
fail:
    for (i = 0; i < dev->num_planes; i++) {
        if (buffer->mem[i] != TD_NULL) {
            free(buffer->mem[i]);
            buffer->mem[i] = TD_NULL;
        }
    }
    return -1;
}

static td_void sample_uvc_video_buffer_free_userptr(const device_info *dev, buffer_info *buffer)
{
    td_u32 i;

    for (i = 0; i < dev->num_planes; i++) {
        free(buffer->mem[i]);
        buffer->mem[i] = TD_NULL;
    }
}

static td_void sample_uvc_video_buffer_fill_userptr(const device_info *dev, buffer_info *buffer,
    struct v4l2_buffer *v4l2buf)
{
    td_u32 i;

    for (i = 0; i < dev->num_planes; i++) {
        v4l2buf->m.planes[i].m.userptr = (td_ulong)(uintptr_t)buffer->mem[i];
    }
}

static td_void sample_uvc_get_ts_flags(td_u32 buf_flag, const td_char **ts_type, const td_char **ts_src)
{
    switch (buf_flag & V4L2_BUF_FLAG_TSTAMP_SRC_MASK) {
        case V4L2_BUF_FLAG_TSTAMP_SRC_SOE:
            *ts_src = "soe";
            break;
        case V4L2_BUF_FLAG_TSTAMP_SRC_EOF:
            *ts_src = "eof";
            break;
        default:
            *ts_src = "inv";
    }

    switch (buf_flag & V4L2_BUF_FLAG_TIMESTAMP_MASK) {
        case V4L2_BUF_FLAG_TIMESTAMP_COPY:
            *ts_type = "copy";
            break;
        case V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC:
            *ts_type = "monotonic";
            break;
        case V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN:
            *ts_type = "unknown";
            break;
        default:
            *ts_type = "inv";
    }
}

static td_s32 sample_uvc_map_buffer(device_info *dev, buffer_info *buffers,
    td_u32 count, td_u32 offset, td_u32 padding)
{
    td_u32 i;
    td_s32 ret;
    struct v4l2_buffer buffer;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];

    for (i = 0; i < count; ++i) {
        const td_char *ts_type;
        const td_char *ts_source;

        (td_void)memset_s(&buffer, sizeof(buffer), 0, sizeof(buffer));
        (td_void)memset_s(planes, sizeof(planes), 0, sizeof(planes));

        buffer.index = i;
        buffer.type = dev->type;
        buffer.memory = dev->memtype;
        buffer.length = VIDEO_MAX_PLANES;
        buffer.m.planes = planes;

        ret = ioctl(dev->fd, VIDIOC_QUERYBUF, &buffer);
        if (ret < 0) {
            sample_print("Unable to query buffer %u\n", i);
            return ret;
        }
        sample_uvc_get_ts_flags(buffer.flags, &ts_type, &ts_source);
        printf("length: %u offset: %u timestamp type/source: %s/%s\n",
            buffer.length, buffer.m.offset, ts_type, ts_source);

        buffers[i].idx = i;

        if (dev->memtype == V4L2_MEMORY_USERPTR) {
            ret = sample_uvc_video_buffer_alloc_userptr(dev, &buffers[i], &buffer, offset, padding);
        } else if (dev->memtype == V4L2_MEMORY_MMAP) {
            ret = sample_uvc_video_buffer_mmap(dev, &buffers[i], &buffer);
        }

        if (ret < 0) {
            return ret;
        }
    }

    dev->timestamp_type = buffer.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
    dev->buffers = buffers;
    dev->nbufs = count;

    return TD_SUCCESS;
}

static td_s32 sample_uvc_video_alloc_buffers(device_info *dev, td_u32 nbufs, td_u32 offset, td_u32 padding)
{
    struct v4l2_requestbuffers rb;
    buffer_info *buffers;
    td_s32 ret;

    (td_void)memset_s(&rb, sizeof(rb), 0, sizeof(rb));
    rb.count = nbufs;
    rb.type = dev->type;
    rb.memory = dev->memtype;

    ret = ioctl(dev->fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        sample_print("Unable to request buffers.\n");
        return ret;
    }

    printf("%u buffers requested.\n", rb.count);

    buffers = calloc(rb.count, sizeof(buffer_info));
    if (buffers == TD_NULL) {
        return -ENOMEM;
    }
    ret = sample_uvc_map_buffer(dev, buffers, rb.count, offset, padding);
    if (ret != TD_SUCCESS) {
        free(buffers);
        return ret;
    }

    return 0;
}

static td_s32 sample_uvc_video_free_buffers(device_info *dev)
{
    struct v4l2_requestbuffers rb = {0};
    td_u32 i;
    td_s32 ret;

    if (dev->nbufs == 0) {
        return 0;
    }

    for (i = 0; i < dev->nbufs; ++i) {
        if (dev->memtype == V4L2_MEMORY_USERPTR) {
            sample_uvc_video_buffer_free_userptr(dev, &dev->buffers[i]);
        } else if (dev->memtype == V4L2_MEMORY_MMAP) {
            ret = sample_uvc_video_buffer_munmap(dev, &dev->buffers[i]);
            if (ret < 0) {
                return ret;
            }
        }
    }

    rb.count = 0;
    rb.type = dev->type;
    rb.memory = dev->memtype;

    ret = ioctl(dev->fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        return ret;
    }

    sample_print("%u buffers released.\n", dev->nbufs);

    free(dev->buffers);
    dev->nbufs = 0;
    dev->buffers = TD_NULL;

    return 0;
}

static td_s32 sample_uvc_video_queue_buffer(device_info *dev, td_u32 index, enum buffer_fill_mode fill)
{
    struct v4l2_buffer buf = {0};
    td_u32 i;

    buf.index = index;
    buf.type = dev->type;
    buf.memory = dev->memtype;

    if (dev->memtype == V4L2_MEMORY_USERPTR) {
        buf.m.userptr = (td_ulong)(uintptr_t)dev->buffers[index].mem[0];
        buf.length = dev->buffers[index].size[0];
    }

    for (i = 0; i < dev->num_planes; i++) {
        if (fill == BUFFER_FILL_FRAME) {
            (td_void)memset_s(dev->buffers[buf.index].mem[i], dev->buffers[index].size[i],
                0x55, dev->buffers[index].size[i]);
        }
        if (fill == BUFFER_FILL_PADDING) {
            (td_void)memset_s(dev->buffers[buf.index].mem[i] + dev->buffers[index].size[i],
                dev->buffers[index].padding[i], 0x55, dev->buffers[index].padding[i]);
        }
    }

    return ioctl(dev->fd, VIDIOC_QBUF, &buf);
}

static td_s32 sample_uvc_video_enable(device_info *dev, td_bool enable)
{
    td_s32 type = dev->type;
    td_s32 ret;

    ret = ioctl(dev->fd, enable ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        sample_print("Unable to %s streaming.\n", enable ? "start" : "stop");
        return ret;
    }

    return 0;
}

static td_void sample_uvc_video_enum_frame_intervals(device_info *dev, td_u32 pixel_format,
    td_u32 width, td_u32 height)
{
    struct v4l2_frmivalenum frmivalenum;
    td_u32 i;
    td_s32 ret;

    for (i = 0; ; ++i) {
        (td_void)memset_s(&frmivalenum, sizeof(frmivalenum), 0, sizeof(frmivalenum));
        frmivalenum.index = i;
        frmivalenum.pixel_format = pixel_format;
        frmivalenum.width = width;
        frmivalenum.height = height;
        ret = ioctl(dev->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum);
        if (ret < 0) {
            break;
        }

        if (i != frmivalenum.index) {
            sample_print("Warning: driver returned wrong ival index %u.\n", frmivalenum.index);
        }
        if (pixel_format != frmivalenum.pixel_format) {
            sample_print("Warning: driver returned wrong ival pixel format %08x.\n", frmivalenum.pixel_format);
        }
        if (width != frmivalenum.width) {
            sample_print("Warning: driver returned wrong ival width %u.\n", frmivalenum.width);
        }
        if (height != frmivalenum.height) {
            sample_print("Warning: driver returned wrong ival height %u.\n", frmivalenum.height);
        }

        if (i != 0) {
            printf(", ");
        }

        switch (frmivalenum.type) {
            case V4L2_FRMIVAL_TYPE_DISCRETE:
                printf("%u/%u", frmivalenum.discrete.numerator, frmivalenum.discrete.denominator);
                break;

            case V4L2_FRMIVAL_TYPE_CONTINUOUS:
                printf("%u/%u - %u/%u", frmivalenum.stepwise.min.numerator, frmivalenum.stepwise.min.denominator,
                    frmivalenum.stepwise.max.numerator, frmivalenum.stepwise.max.denominator);
                return;

            case V4L2_FRMIVAL_TYPE_STEPWISE:
                printf("%u/%u - %u/%u (by %u/%u)", frmivalenum.stepwise.min.numerator,
                    frmivalenum.stepwise.min.denominator, frmivalenum.stepwise.max.numerator,
                    frmivalenum.stepwise.max.denominator, frmivalenum.stepwise.step.numerator,
                    frmivalenum.stepwise.step.denominator);
                return;

            default:
                break;
        }
    }
}

static td_void sample_uvc_video_enum_frame_sizes(device_info *dev, td_u32 pixelformat)
{
    struct v4l2_frmsizeenum frame = {0};
    td_u32 i = 0;
    td_s32 ret;

    while (TD_TRUE) {
        frame.pixel_format = pixelformat;
        frame.index = i;
        ret = ioctl(dev->fd, VIDIOC_ENUM_FRAMESIZES, &frame);
        if (ret != TD_SUCCESS) {
            break;
        }

        if (pixelformat != frame.pixel_format) {
            sample_print("got wrong frame pixel format %08x.\n", frame.pixel_format);
        }
        if (i != frame.index) {
            sample_print("got wrong frame index %u.\n", frame.index);
        }

        switch (frame.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                printf("\tFrame size: %ux%u (", frame.discrete.width, frame.discrete.height);
                sample_uvc_video_enum_frame_intervals(dev, frame.pixel_format, frame.discrete.width,
                    frame.discrete.height);
                printf(")\n");
                break;

            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                printf("\tFrame size: %ux%u - %ux%u (", frame.stepwise.min_width, frame.stepwise.min_height,
                    frame.stepwise.max_width, frame.stepwise.max_height);
                sample_uvc_video_enum_frame_intervals(dev, frame.pixel_format,
                    frame.stepwise.max_width, frame.stepwise.max_height);
                printf(")\n");
                break;

            case V4L2_FRMSIZE_TYPE_STEPWISE:
                printf("\tFrame size: %ux%u - %ux%u (by %ux%u) (\n",
                    frame.stepwise.min_width, frame.stepwise.min_height,
                    frame.stepwise.max_width, frame.stepwise.max_height,
                    frame.stepwise.step_width, frame.stepwise.step_height);
                sample_uvc_video_enum_frame_intervals(dev, frame.pixel_format,
                    frame.stepwise.max_width, frame.stepwise.max_height);
                printf(")\n");
                break;

            default:
                break;
        }
        i++;
    }
}

static td_void sample_uvc_video_enum_formats(device_info *dev, enum v4l2_buf_type type)
{
    struct v4l2_fmtdesc fmt = {0};
    td_u32 i = 0;
    td_s32 ret;

    while (TD_TRUE) {
        fmt.type = type;
        fmt.index = i;
        ret = ioctl(dev->fd, VIDIOC_ENUM_FMT, &fmt);
        if (ret < 0) {
            break;
        }

        if (i != fmt.index) {
            sample_print("got wrong format index %u.\n", fmt.index);
        }
        if (type != fmt.type) {
            sample_print("got wrong format type %u.\n", fmt.type);
        }

        printf("\tFormat %u: %s (%08x)\n", i, sample_uvc_v4l2_format_name(fmt.pixelformat), fmt.pixelformat);
        printf("\tType: %s (%u)\n", sample_uvc_v4l2_buf_type_name(fmt.type), fmt.type);
        printf("\tName: %.32s\n", fmt.description);
        sample_uvc_video_enum_frame_sizes(dev, fmt.pixelformat);
        printf("\n");

        i++;
    }
}

static td_s32 sample_uvc_video_prepare_capture(device_info *dev, td_u32 nbufs, td_u32 offset,
    const td_char *filename, enum buffer_fill_mode fill)
{
    td_u32 padding;
    td_s32 ret;

    padding = (fill == BUFFER_FILL_PADDING) ? 4096 : 0;  /* 4096:padding */
    if ((ret = sample_uvc_video_alloc_buffers(dev, nbufs, offset, padding)) < 0) {
        return ret;
    }

    return 0;
}

static td_s32 sample_uvc_video_queue_all_buffers(device_info *dev, enum buffer_fill_mode fill)
{
    td_u32 i;
    td_s32 ret;

    for (i = 0; i < dev->nbufs; ++i) {
        ret = sample_uvc_video_queue_buffer(dev, i, fill);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static td_void sample_uvc_video_save_image(const device_info *dev, const struct v4l2_buffer *buf,
    td_u32 sequence, uvc_ctrl_info *ctrl_info)
{
    td_u32 i;
    ot_size pic_size;
    td_u32 stride;
    const td_char *type_name = ctrl_info->type_name;

    pic_size.width = ctrl_info->width;
    pic_size.height = ctrl_info->height;
    stride = dev->plane_fmt[0].bytesperline;

    for (i = 0; i < dev->num_planes; i++) {
        td_void *data = dev->buffers[buf->index].mem[i];
        td_u32 length = buf->bytesused;

        if (sample_uvc_media_send_data(data, length, stride, &pic_size, type_name) != TD_SUCCESS) {
            sample_print("media_send_data failed!\n");
        }
    }
}

static td_void sample_uvc_get_buf_cfg(device_info *dev, struct v4l2_plane planes[], td_u32 len, struct v4l2_buffer *buf)
{
    ot_unused(len);

    (td_void)memset_s(buf, sizeof(struct v4l2_buffer), 0, sizeof(struct v4l2_buffer));

    buf->type = dev->type;
    buf->memory = dev->memtype;
    buf->length = VIDEO_MAX_PLANES;
    buf->m.planes = planes;
}

static td_s32 sample_uvc_dqueue_buf(device_info *dev, td_u32 index, struct v4l2_buffer *buf)
{
    if (ioctl(dev->fd, VIDIOC_DQBUF, buf) < 0) {
        if (errno != EIO) {
            return TD_FAILURE;
        }
        buf->type = dev->type;
        buf->memory = dev->memtype;
        if (dev->memtype == V4L2_MEMORY_USERPTR) {
            sample_uvc_video_buffer_fill_userptr(dev, &dev->buffers[index], buf);
        }
    }
    return TD_SUCCESS;
}

static td_void sample_uvc_print_fps(struct timespec *start, td_u32 size, td_u32 frame_num, struct timespec *ts)
{
    td_double bps;
    td_double fps;

    ts->tv_sec -= start->tv_sec;
    ts->tv_nsec -= start->tv_nsec;
    if (ts->tv_nsec < 0) {
        ts->tv_sec--;
        ts->tv_nsec += 1000000000;  /* 1000000000:time */
    }

    bps = size / (ts->tv_nsec / 1000.0 + 1000000.0 * ts->tv_sec) * 1000000.0;       /* 1000.0,1000000.0:time */
    fps = frame_num / (ts->tv_nsec / 1000.0 + 1000000.0 * ts->tv_sec) * 1000000.0;  /* 1000.0,1000000.0:time */

    sample_print("Captured %u frames in %ld.%06ld seconds (%f fps, %f B/s).\n",
        frame_num, (td_slong)ts->tv_sec, ts->tv_nsec / 1000, fps, bps);                       /* 1000:time */
}

static td_s32 sample_uvc_capture(device_info *dev, uvc_ctrl_info *ctrl_info,
    struct timespec *ts, td_u32 *frame_num, td_u32 *size)
{
    td_u32 i;
    td_s32 ret;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer buf;
    td_u32 skip = ctrl_info->skip;

    for (i = 0; i < ctrl_info->nframes; ++i) {
        if (g_uvc_exit == TD_TRUE) {
            break;
        }

        (td_void)memset_s(planes, sizeof(planes), 0, sizeof(planes));

        sample_uvc_get_buf_cfg(dev, planes, VIDEO_MAX_PLANES, &buf);

        ret = sample_uvc_dqueue_buf(dev, i, &buf);
        if (ret != TD_SUCCESS) {
            return TD_FAILURE;
        }

        *size += buf.bytesused;

        clock_gettime(CLOCK_MONOTONIC, ts);

        if (sample_uvc_video_is_capture(dev) && ctrl_info->pattern && skip == 0) {
            sample_uvc_video_save_image(dev, &buf, i, ctrl_info);
        }

        if (skip > 0) {
            --skip;
        }

        if (ctrl_info->delay > 0) {
            usleep(ctrl_info->delay * 1000);    /* 1000:time */
        }

        (td_void)fflush(stdout);

        if (i >= ctrl_info->nframes - dev->nbufs && !ctrl_info->do_requeue_last) {
            continue;
        }

        ret = sample_uvc_video_queue_buffer(dev, buf.index, ctrl_info->fill);
        if (ret < 0) {
            sample_print("Unable to requeue buffer.\n");
            return TD_FAILURE;
        }
    }

    *frame_num = i;

    return TD_SUCCESS;
}

static td_s32 sample_uvc_video_do_capture(device_info *dev, uvc_ctrl_info *ctrl_info)
{
    struct timespec start = {0};
    struct timespec ts = {0};
    td_u32 size;
    td_s32 ret;
    td_u32 frame_num;

    ret = sample_uvc_video_enable(dev, TD_TRUE);
    if (ret < 0) {
        goto done;
    }

    if (ctrl_info->do_queue_late) {
        sample_uvc_video_queue_all_buffers(dev, ctrl_info->fill);
    }

    size = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ret = sample_uvc_capture(dev, ctrl_info, &ts, &frame_num, &size);
    if (ret != TD_SUCCESS) {
        goto done;
    }

    ret = sample_uvc_video_enable(dev, TD_FALSE);
    if (ret < 0) {
        return ret;
    }

    if (ctrl_info->nframes == 0) {
        sample_print("No frames captured.\n");
        goto done;
    }

    if (ts.tv_sec == start.tv_sec && ts.tv_nsec == start.tv_nsec) {
        sample_print("Captured %u frames (%u bytes) 0 seconds\n", frame_num, size);
        goto done;
    }

    sample_uvc_print_fps(&start, size, frame_num, &ts);

done:
    return sample_uvc_video_free_buffers(dev);
}

static td_void sample_uvc_usage(const td_char *argv0)
{
    printf("sample_uvc_usage: %s device [options]\n", argv0);
    printf("supported options:\n");
    printf("-f, --format format             set the video format\n");
    printf("-F, --file[=name]               write file\n");
    printf("-h, --help                      show help info\n");
    printf("-s, --size WxH                  set the frame size (eg. 1920x1080)\n\n");
    printf("inquire USB device format: ./sample_host_uvc /dev/video0 --enum-formats\n\n");
    printf("example of setting USB device format:\n");
    printf("    ./sample_host_uvc /dev/video0 -fH264  -s1920x1080 -Ftest.h264\n");
    printf("    ./sample_host_uvc /dev/video0 -fH265  -s1920x1080 -Ftest.h265\n");
    printf("    ./sample_host_uvc /dev/video0 -fMJPEG -s1920x1080 -Ftest.mjpg\n");
    printf("    ./sample_host_uvc /dev/video0 -fYUYV  -s1920x1080 -Ftest.yuv\n");
    printf("    ./sample_host_uvc /dev/video0 -fNV21  -s640x360   -Ftest.yuv\n\n");
}

static const td_char *sample_uvc_video_get_data_type(device_info *dev)
{
    struct v4l2_format fmt;
    td_s32 ret;
    (td_void)memset_s(&fmt, sizeof(fmt), 0, sizeof(fmt));
    fmt.type = dev->type;

    ret = ioctl(dev->fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        sample_print("Unable to get format.\n");
        return TD_NULL;
    }

    return sample_uvc_v4l2_format_name(fmt.fmt.pix_mp.pixelformat);
}

static td_s32 sample_uvc_get_opt(td_s32 argc, td_char *argv[], device_info *dev, uvc_ctrl_info *ctrl_info)
{
    td_s32 c;
    td_char *endptr;

    while ((c = getopt_long(argc, argv, "f:F:s:h", g_opts, TD_NULL)) != -1) {
        switch (c) {
            case 'f':
                if (strcmp("help", optarg) == 0) {
                    sample_uvc_list_formats();
                    return TD_FAILURE;
                }
                ctrl_info->do_set_format = TD_TRUE;
                const format_info *info = sample_uvc_v4l2_format_by_name(optarg);
                if (info == TD_NULL) {
                    sample_print("Unsupported video format '%s'\n", optarg);
                    return TD_FAILURE;
                }
                ctrl_info->pixelformat = info->fourcc;
                break;
            case 'F':
                ctrl_info->do_file = TD_TRUE;
                if (optarg != TD_NULL) {
                    ctrl_info->pattern = optarg;
                }
                break;
            case 's':
                ctrl_info->do_capture = TD_TRUE;
                ctrl_info->do_set_format = TD_TRUE;
                ctrl_info->width = (td_u32)strtol(optarg, &endptr, 10); /* 10:base */
                if (*endptr != 'x' || endptr == optarg) {
                    sample_print("Invalid size '%s'\n", optarg);
                    return TD_FAILURE;
                }
                ctrl_info->height = (td_u32)strtol(endptr + 1, &endptr, 10);    /* 10:base */
                if (*endptr != 0) {
                    sample_print("Invalid size '%s'\n", optarg);
                    return TD_FAILURE;
                }
                break;
            case SAMPLE_UVC_OPT_ENUM_FORMATS:
                ctrl_info->do_enum_formats = TD_TRUE;
                break;
            case 'h':
            default:
                sample_uvc_usage(argv[0]);
                return TD_FAILURE;
        }
    }
    return TD_SUCCESS;
}

static td_void sample_uvc_ctrl_info_init(uvc_ctrl_info *ctrl_info)
{
    ctrl_info->time_per_frame.numerator = 1;
    ctrl_info->time_per_frame.denominator = 25;     /* 25 frames per second */
    ctrl_info->pattern = "frame-#.bin";
    ctrl_info->nbufs = SAMPLE_UVC_V4L_BUFFERS_DEFAULT;
    ctrl_info->memtype = V4L2_MEMORY_MMAP;
    ctrl_info->nframes = (td_u32)-1;
    ctrl_info->width = 640;     /* 640:width */
    ctrl_info->height = 480;    /* 480:height */
    ctrl_info->pixelformat = V4L2_PIX_FMT_YUYV;
    ctrl_info->fill = BUFFER_FILL_NONE;
    ctrl_info->rt_priority = 1;
}

static td_s32 sample_uvc_prepare_to_set_ctrl(td_s32 argc, td_char *argv[], device_info *dev, uvc_ctrl_info *ctrl_info,
    td_s32 *type)
{
    td_s32 ret;
    td_u32 capabilities = V4L2_CAP_VIDEO_CAPTURE;

    if ((ctrl_info->fill == BUFFER_FILL_PADDING) && (ctrl_info->memtype != V4L2_MEMORY_USERPTR)) {
        sample_print("Buffer overrun can only be checked in USERPTR mode.\n");
        return TD_FAILURE;
    }

    if (!sample_uvc_video_has_fd(dev)) {
        if (optind >= argc) {
            sample_uvc_usage(argv[0]);
            return TD_FAILURE;
        }
        ret = sample_uvc_video_open(dev, argv[optind]);
        if (ret < 0) {
            return TD_FAILURE;
        }
    }

    if (!ctrl_info->no_query) {
        ret = sample_uvc_video_query_cap(dev, &capabilities);
        if (ret < 0) {
            return TD_FAILURE;
        }
    }

    *type = sample_uvc_cap_get_buf_type(capabilities);
    if (*type < 0) {
        return TD_FAILURE;
    }

    if (!ctrl_info->do_file) {
        ctrl_info->pattern = TD_NULL;
    }

    return TD_SUCCESS;
}

static td_void sample_uvc_set_buf_type(device_info *dev, uvc_ctrl_info *ctrl_info, td_s32 type)
{
    if (!sample_uvc_video_has_valid_buf_type(dev)) {
        sample_uvc_video_set_buf_type(dev, type);
    }

    dev->memtype = ctrl_info->memtype;
}

static td_s32 sample_uvc_set_video_format(device_info *dev, uvc_ctrl_info *ctrl_info)
{
    if (ctrl_info->do_enum_formats) {
        printf("- Available formats:\n");
        sample_uvc_video_enum_formats(dev, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        sample_uvc_video_enum_formats(dev, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    }

    if (ctrl_info->do_set_format) {
        if (sample_uvc_video_set_format(dev, ctrl_info) < 0) {
            sample_uvc_video_close(dev);
            return TD_FAILURE;
        }
    }

    if (!ctrl_info->no_query || ctrl_info->do_capture) {
        sample_uvc_video_get_format(dev);
    }

    if (ctrl_info->do_set_time_per_frame) {
        if (sample_uvc_video_set_frame_rate(dev, &ctrl_info->time_per_frame) < 0) {
            sample_uvc_video_close(dev);
            return TD_FAILURE;
        }
    }

    return TD_SUCCESS;
}

static td_bool sample_uvc_is_close_video(device_info *dev, uvc_ctrl_info *ctrl_info)
{
    struct sched_param sched;
    td_s32 ret;

    if (!ctrl_info->do_capture) {
        sample_uvc_video_close(dev);
        return TD_TRUE;
    }

    if (sample_uvc_video_prepare_capture(dev, ctrl_info->nbufs, ctrl_info->userptr_offset,
        ctrl_info->pattern, ctrl_info->fill) != TD_SUCCESS) {
        sample_uvc_video_close(dev);
        return TD_TRUE;
    }

    if (!ctrl_info->do_queue_late && sample_uvc_video_queue_all_buffers(dev, ctrl_info->fill) != TD_SUCCESS) {
        sample_uvc_video_close(dev);
        return TD_TRUE;
    }

    if (ctrl_info->do_rt) {
        (td_void)memset_s(&sched, sizeof(sched), 0, sizeof(sched));
        sched.sched_priority = ctrl_info->rt_priority;
        ret = sched_setscheduler(0, SCHED_RR, &sched);
        if (ret < 0) {
            sample_print("Failed to select RR scheduler\n");
        }
    }

    return TD_FALSE;
}

td_s32 main(td_s32 argc, td_char *argv[])
{
    device_info dev = {0};
    td_s32 ret;
    td_s32 type;
    uvc_ctrl_info ctrl_info = {0};

    struct sigaction sig_exit = {0};
    sig_exit.sa_handler = sample_uvc_exit_signal_handler;
    sigaction(SIGINT, &sig_exit, TD_NULL);
    sigaction(SIGTERM, &sig_exit, TD_NULL);

    sample_uvc_ctrl_info_init(&ctrl_info);

    sample_uvc_video_init(&dev);

    opterr = 0;

    ret = sample_uvc_get_opt(argc, argv, &dev, &ctrl_info);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    ret = sample_uvc_prepare_to_set_ctrl(argc, argv, &dev, &ctrl_info, &type);
    if (ret != TD_SUCCESS) {
        return ret;
    }
    sample_uvc_set_buf_type(&dev, &ctrl_info, type);

    ret = sample_uvc_set_video_format(&dev, &ctrl_info);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    if (sample_uvc_is_close_video(&dev, &ctrl_info) == TD_TRUE) {
        return TD_FAILURE;
    }

    ret = sample_uvc_media_init(sample_uvc_video_get_data_type(&dev), ctrl_info.width, ctrl_info.height,
        ctrl_info.pattern);
    if (ret != TD_SUCCESS) {
        sample_print("media start failed!\n");
        goto video_close;
    }

    ctrl_info.type_name = sample_uvc_v4l2_format_name(ctrl_info.pixelformat);
    if (sample_uvc_video_do_capture(&dev, &ctrl_info) < 0) {
        ret = TD_FAILURE;
    }

    sample_uvc_media_stop_receive_data();
    sample_uvc_media_exit();
    printf("media exit...\n");

video_close:
    sample_uvc_video_close(&dev);
    return ret;
}
