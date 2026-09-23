/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/select.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <linux/usb/ch9.h>
#include "securec.h"

#include "ot_camera.h"
#include "uvc.h"
#include "ot_stream.h"
#include "frame_cache.h"
#ifdef UVC_STILL_IMAGE
#include "uvc_media.h"
#endif

#undef OT_DEBUG

#ifdef OT_DEBUG
#define debug(fmt, args...) printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

#define ERR_INFO printf
#define RIGHT_SHIFT_8BIT 8U
#define UDC_PATH_LENGTH 389
#define GET_MAX_PACKET_SIZE_PATH 398
#define SELECT_DELAY_USECS 40000

static struct ot_uvc_dev *g_ot_cd[OT_UVC_MAX_CHN_NUM] = {0};
static td_void ot_uvc_get_buf_size(struct ot_uvc_dev *uvc_dev, td_u32 width, td_u32 height, td_u32 fcc,
    td_u32 *dw_max_video_frame_size);

static const td_char *image_fmt_to_string(td_u32 format)
{
    switch (format) {
        case VIDEO_IMG_FORMAT_H264:
            return "H264";
        case VIDEO_IMG_FORMAT_H265:
            return "H265";
        case VIDEO_IMG_FORMAT_MJPEG:
            return "MJPEG";
        case VIDEO_IMG_FORMAT_YUYV:
            return "YUYV";
        case VIDEO_IMG_FORMAT_YUV420:
            return "YUV420";
        case VIDEO_IMG_FORMAT_NV21:
            return "NV21";
        case VIDEO_IMG_FORMAT_NV12:
            return "NV12";
        default:
            return "unknown format";
    }
}

#ifdef OT_DEBUG
static const td_char *get_code(td_s32 code)
{
    if (code == 0x01) { // 0x01 == SET_CUR
        return "SET_CUR";
    } else if (code == 0x81) { // 0x81 == GET_CUR
        return "GET_CUR";
    } else if (code == 0x82) { // 0x82 == GET_MIN
        return "GET_MIN";
    } else if (code == 0x83) { // 0x83 == GET_MAX
        return "GET_MAX";
    } else if (code == 0x84) { // 0x84 == GET_RES
        return "GET_RES";
    } else if (code == 0x85) { // 0x85 == GET_LEN
        return "GET_LEN";
    } else if (code == 0x86) { // 0x86 == GET_INFO
        return "GET_INFO";
    } else if (code == 0x87) { // 0x87 == GET_DEF
        return "GET_DEF";
    } else {
        return "UNKNOWN";
    }
}

static const td_char *get_intf_cs_string(td_s32 code)
{
    if (code == 0x01) {
        return "PROB_CONTROL";
    } else if (code == 0x02) {
        return "COMMIT_CONTROL";
    } else {
        return "UNKNOWN";
    }
}
#endif
static td_void clear_uvc_frame_nodes(struct ot_uvc_dev *uvc_dev)
{
    td_s32 i;
    uvc_cache_t *uvc_cache = uvc_dev->uvc_cache;

    if (uvc_cache == NULL) {
        return ;
    }
    for (i = 0; i < MAX_SAVE_FRAME_NUMS; i++) {
        if (uvc_dev->frame_node[i]) {
            put_node_to_queue(uvc_cache->free_queue, uvc_dev->frame_node[i]);
            uvc_dev->frame_node[i] = NULL;
        }
    }
}
static struct ot_uvc_dev *ot_uvc_init(const td_char *dev_name, td_u32 dev_no)
{
    struct ot_uvc_dev *uvc_dev = NULL;
    struct video_ability cap;
    td_s32 ret;
    td_s32 fd;

    td_char *regular_path = realpath(dev_name, NULL);
    if (regular_path == NULL) {
        ERR_INFO("unable to get real path: %s\n", dev_name);
        return NULL;
    }
    fd = open(regular_path, O_RDWR | O_NONBLOCK);
    free(regular_path);
    if (fd == -1) {
        ERR_INFO("v4l2 open failed(%s): %d\n", dev_name, errno);
        return NULL;
    }

    ret = ioctl(fd, VIDEO_IOCTL_QUERY_CAP, &cap);
    if (ret < 0) {
        ERR_INFO("unable to query device: %d\n", errno);
        close(fd);
        return NULL;
    }

    debug("open succeeded(%s:caps=0x%04x)\n", dev_name, cap.dw_caps);

    if (!(cap.dw_caps & 0x02)) {
        close(fd);
        return NULL;
    }

    debug("device is %s on bus %s\n", cap.a_card, cap.a_bus_info);

    uvc_dev = (struct ot_uvc_dev *)malloc(sizeof *uvc_dev);
    if (uvc_dev == NULL) {
        close(fd);
        return NULL;
    }

    (td_void)memset_s(uvc_dev, sizeof(*uvc_dev), 0, sizeof(*uvc_dev));
    uvc_dev->i_fd = fd;
    uvc_dev->dev_no = dev_no;

    return uvc_dev;
}

static td_void ot_uvc_video_fill_buffer_userptr(struct ot_uvc_dev *uvc_dev, struct video_cache *v_cache)
{
    td_s32 s32_retry_count = 0;
    v_cache->dw_used = 0;

    switch (uvc_dev->dw_fcc) {
        case VIDEO_IMG_FORMAT_MJPEG:
        case VIDEO_IMG_FORMAT_H264:
        case VIDEO_IMG_FORMAT_H265:
        case VIDEO_IMG_FORMAT_NV21:
        case VIDEO_IMG_FORMAT_NV12:
        case VIDEO_IMG_FORMAT_YUYV:
        case VIDEO_IMG_FORMAT_YUV420: {
            uvc_cache_t *c_uvc_cache = uvc_cache_get(uvc_dev->dev_no);
            frame_node_t *c_node = 0;
            frame_queue_t *c_q = 0;
            frame_queue_t *c_fq = 0;
            do {
                if (c_uvc_cache != NULL) {
                    c_q = c_uvc_cache->ok_queue;
                    c_fq = c_uvc_cache->free_queue;
                    get_node_from_queue(c_q, &c_node);
                }

                if ((uvc_dev->frame_node[v_cache->dw_index] != 0) && (c_uvc_cache  != NULL)) {
                    put_node_to_queue(c_fq, uvc_dev->frame_node[v_cache->dw_index]);
                    uvc_dev->frame_node[v_cache->dw_index] = TD_NULL;
                }

                if (c_node != 0) {
                    v_cache->dw_used = c_node->used;
                    v_cache->n.ul_usr = (td_ulong)(td_uintptr_t)c_node->mem;
                    v_cache->dw_len = c_node->length;
                    uvc_dev->frame_node[v_cache->dw_index] = c_node;
                    break;
                } else {
                    /*
                    * Notice, the perfect solution is using locker and waiting queue's notify.
                    * But here just only simply used usleep method and try again.
                    * it works fine now.
                    */
                    usleep(5000); // delay 5000 us
                    s32_retry_count++;
                }
            } while (s32_retry_count < 30); // Retry 30 times
        }
            break;
        default:
            break;
    }
}

static td_s32 ot_uvc_video_process_user_ptr(struct ot_uvc_dev *uvc_dev)
{
    struct video_cache v_cache;
    td_s32 ret;

    if (uvc_dev->i_streaming == 0) {
        return 0;
    }
    (td_void)memset_s(&v_cache, sizeof(v_cache), 0, sizeof(v_cache));
    v_cache.dw_type = VIDEO_CACHE_TYPE_OPT;
    v_cache.dw_mem = VIDEO_MM_USER;

    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DQUEUE_BUF, &v_cache);
    if (ret < 0) {
        return ret;
    }

    ot_uvc_video_fill_buffer_userptr(uvc_dev, &v_cache);

    if (uvc_dev->i_streaming == 0) {
        return 0;
    }

    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_QUEUE_BUF, &v_cache);
    if (ret < 0) {
        ERR_INFO("Unable to requeue buffer(1): %d\n", errno);
        return ret;
    }
    usleep(4000); /* 4000 sleep 4ms give up CPU */

    return 0;
}

static td_s32 ot_uvc_video_reqbufs_user_ptr(struct ot_uvc_dev *uvc_dev, td_u32 num_bufs)
{
    struct video_buf_request v_rb;
    td_s32 s32_ret;

    uvc_dev->dw_nbufs = 0;

    (td_void)memset_s(&v_rb, sizeof(v_rb), 0, sizeof(v_rb));
    v_rb.dw_cnt = num_bufs;
    v_rb.dw_type = VIDEO_CACHE_TYPE_OPT;
    v_rb.dw_mem = VIDEO_MM_USER;

    s32_ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_REQUEST_BUF, &v_rb);
    if (s32_ret < 0) {
        ERR_INFO("Unable to allocate buffers: %d\n", errno);
        return s32_ret;
    }

    uvc_dev->dw_nbufs = v_rb.dw_cnt;

    return 0;
}

static td_void ot_uvc_video_stream_user_ptr(struct ot_uvc_dev *uvc_dev)
{
    struct video_cache v_cache;
    td_u32 i;
    td_s32 cache_type = VIDEO_CACHE_TYPE_OPT;
    td_s32 ret;

    for (i = 0; i < (uvc_dev->dw_nbufs); ++i) {
        (td_void)memset_s(&v_cache, sizeof(v_cache), 0, sizeof(v_cache));

        v_cache.dw_index = i;
        v_cache.dw_type = VIDEO_CACHE_TYPE_OPT;
        v_cache.dw_mem = VIDEO_MM_USER;

        ot_uvc_video_fill_buffer_userptr(uvc_dev, &v_cache);

        ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_QUEUE_BUF, &v_cache);
        if (ret < 0) {
            ERR_INFO("Unable to queue buffer: %d\n", errno);
            return;
        }
    }

    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_STREAM_ON, &cache_type);
    if (ret < 0) {
        ERR_INFO("Unable to stream on: %d\n", errno);
        return;
    }

    uvc_dev->i_streaming = 1;
    return;
}

static td_s32 ot_uvc_video_set_fmt(struct ot_uvc_dev *uvc_dev)
{
    struct video_fmt v_fmt;
    td_s32 ret;

    (td_void)memset_s(&v_fmt, sizeof(v_fmt), 0, sizeof(v_fmt));
    v_fmt.dw_type = VIDEO_CACHE_TYPE_OPT;
    v_fmt.fmt.pix.dw_width = uvc_dev->dw_width;
    v_fmt.fmt.pix.dw_height = uvc_dev->dw_height;
    v_fmt.fmt.pix.dw_fmt = uvc_dev->dw_fcc;
    v_fmt.fmt.pix.dw_fld = VIDEO_FLD_NOTHING;

    if ((uvc_dev->dw_fcc == VIDEO_IMG_FORMAT_MJPEG) ||
        (uvc_dev->dw_fcc == VIDEO_IMG_FORMAT_H264) ||
        (uvc_dev->dw_fcc == VIDEO_IMG_FORMAT_H265)) {
        v_fmt.fmt.pix.dw_size = uvc_dev->dw_width * uvc_dev->dw_height / 2; /* 2 half for encoded data */
    }

    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_SET_FORMAT, &v_fmt);
    if (ret < 0) {
        ERR_INFO("Unable to set format: %d\n", errno);
    }

    return ret;
}

static td_void ot_uvc_stream_off(struct ot_uvc_dev *uvc_dev)
{
    td_s32 cache_type = VIDEO_CACHE_TYPE_OPT;
    td_s32 ret;

    uvc_dev->i_streaming = 0;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_STREAM_OFF, &cache_type);
    if (ret < 0) {
        ERR_INFO("Unable to stream off: %d\n", errno);
    }

    ot_stream_shutdown(uvc_dev->dev_no);
    debug("Stopping video stream.\n");
}

static td_void ot_uvc_video_disable(struct ot_uvc_dev *uvc_dev)
{
    uvc_frame_nodes_free(uvc_dev->uvc_cache);

    ot_uvc_stream_off(uvc_dev);
    clear_uvc_frame_nodes(uvc_dev);
}

static td_void ot_uvc_video_enable(struct ot_uvc_dev *uvc_dev)
{
    encoder_property ep;
    td_u32 max_video_frame_size = 0;

    clear_uvc_frame_nodes(uvc_dev);
    ot_uvc_video_disable(uvc_dev);

    ep.format = uvc_dev->dw_fcc;
    ep.width = uvc_dev->dw_width;
    ep.height = uvc_dev->dw_height;
    ep.fps = uvc_dev->i_fps;
    ep.compsite = 0;

    ot_uvc_get_buf_size(uvc_dev, ep.width, ep.height, ep.format, &max_video_frame_size);
    uvc_frame_nodes_alloc(uvc_dev->uvc_cache, max_video_frame_size);
    
    ot_stream_set_enc_property(uvc_dev->dev_no, &ep);
    ot_stream_shutdown(uvc_dev->dev_no);
    ot_stream_startup(uvc_dev->dev_no);
    ot_uvc_video_reqbufs_user_ptr(uvc_dev, MAX_SAVE_FRAME_NUMS);
    ot_uvc_video_stream_user_ptr(uvc_dev);
}

static td_s32 is_contain_udc_sub_dir(const td_char *path, td_s32 path_size)
{
    DIR *p_dir = NULL;
    struct dirent *p_dirent = NULL;
    if (path_size < 0) {
        return -1;
    }

    p_dir = opendir(path);
    if (p_dir == NULL) {
        ERR_INFO("No such path: %s\n", path);
        return -1;
    }

    while ((p_dirent = readdir(p_dir)) != NULL) {
        if (strcmp(p_dirent->d_name, "UDC") == 0) {
            closedir(p_dir);
            return 0;
        }
    }

    closedir(p_dir);

    return -1;
}

static td_s32 read_file(const td_char *path, td_u32 path_size, const td_char *dest, td_u32 size)
{
    FILE *input = NULL;
    td_char *realpath_res = NULL;
    td_s32 ret;

    realpath_res = realpath(path, NULL);
    if (realpath_res == NULL) {
        return -1;
    }

    input = fopen(realpath_res, "rw");
    free(realpath_res);
    realpath_res = NULL;
    if (input == NULL) {
        ERR_INFO("No such path: %s\n", path);
        return -1;
    }

    ret = fscanf_s(input, "%s", dest, size);
    if (ret != 1) {
        ERR_INFO("fscanf_s from %s fail\n", path);
        ret = TD_FAILURE;
    } else {
        ret = TD_SUCCESS;
    }

    if (fclose(input) != 0) {
        ERR_INFO("close file failed!\n");
    }

    return ret;
}

static td_s32 get_udc_node_name(td_char *node_name, td_u32 size)
{
    DIR *p_dir = NULL;
    struct dirent *p_dirent = NULL;
    const td_char *path_tmp = "/sys/kernel/config/usb_gadget/";
    td_char tmp[UDC_PATH_LENGTH] = {0};

    p_dir = opendir(path_tmp);
    if (p_dir == NULL) {
        ERR_INFO("No such path: %s\n", path_tmp);
        return -1;
    }

    while ((p_dirent = readdir(p_dir)) != NULL) {
        if (strcmp(p_dirent->d_name, ".") == 0 || strcmp(p_dirent->d_name, "..") == 0) {
            continue;
        }

        if (p_dirent->d_type == DT_DIR) {
            snprintf_truncated_s(tmp, UDC_PATH_LENGTH, "%s%s", path_tmp, p_dirent->d_name);

            if (is_contain_udc_sub_dir(tmp, UDC_PATH_LENGTH) == 0) {
                snprintf_truncated_s(tmp, UDC_PATH_LENGTH, "%s%s/UDC", path_tmp, p_dirent->d_name);
                closedir(p_dir);
                return read_file(tmp, UDC_PATH_LENGTH, node_name, size);
            }
        }
    }

    closedir(p_dir);

    return -1;
}

static td_u32 get_max_payload_transfer_size(const struct ot_uvc_frame_info *frm_info)
{
    const td_u32 high_speed_size = 3072;
    const td_u32 full_speed_size = 1023;
    const td_char *path_tmp = "/sys/class/udc/";

    td_char tmp[128];
    td_char target_path[GET_MAX_PACKET_SIZE_PATH]; // 398=256+128+14
    td_u32 result = high_speed_size;

    if (get_udc_node_name(tmp, 128) != 0) { /* 128 bytes used for tmp */
        return result;
    }

    snprintf_truncated_s(target_path, GET_MAX_PACKET_SIZE_PATH, "%s%s/current_speed", path_tmp, tmp);

    if (read_file(target_path, GET_MAX_PACKET_SIZE_PATH, tmp, 128) != 0) { /* 128 bytes used for tmp */
        return result;
    }

    if (strcmp(tmp, "super-speed") == 0) {
        result = frm_info->ss_xfersize;
    } else if (strcmp(tmp, "high-speed") == 0) {
        result = frm_info->hs_xfersize;
    } else if (strcmp(tmp, "full-speed") == 0) {
        result = full_speed_size;
    } else {
        ERR_INFO("USB cable is not connected yet.\n");
    }

    return result;
}

static td_void ot_uvc_get_buf_size(struct ot_uvc_dev *uvc_dev, td_u32 width, td_u32 height, td_u32 fcc,
    td_u32 *dw_max_video_frame_size)
{
    switch (fcc) {
        case VIDEO_IMG_FORMAT_YUYV:
            *dw_max_video_frame_size = width * height * 2; /* 2 bytes a pixel */
            break;
        case VIDEO_IMG_FORMAT_YUV420:
        case VIDEO_IMG_FORMAT_NV12:
        case VIDEO_IMG_FORMAT_NV21:
            *dw_max_video_frame_size = width * height * 3 / 2; /* 3 / 2,  1.5 byte a pixel */
            break;
        case VIDEO_IMG_FORMAT_MJPEG:
        case VIDEO_IMG_FORMAT_H264:
        case VIDEO_IMG_FORMAT_H265:
            *dw_max_video_frame_size = uvc_dev->dw_img_size;
            break;
        default:
            printf("format error!\n");
            break;
    }
}

#ifdef UVC_STILL_IMAGE
static td_u32 clamp(td_u32 val, td_u32 min, td_u32 max);

static td_u32 is_still_image(td_u32 cs)
{
    return (cs == OT_UVC_VS_STILL_PROBE_CONTROL || cs == OT_UVC_VS_STILL_COMMIT_CONTROL ||
        cs == OT_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL);
}

static td_u32 ot_get_still_capture_method(td_u32 dev_no)
{
    td_u32 result = 0;
    td_char char_tmp[128] = {0};
    td_char file_path[128] = {0};
    (td_void)snprintf_truncated_s(file_path, sizeof(file_path),
        "/sys/kernel/config/usb_gadget/camera/functions/uvc.usb%u/still_capture_method", dev_no);

    if (read_file(file_path, GET_MAX_PACKET_SIZE_PATH, char_tmp, 128) != 0) { /* 128 bytes used for char_tmp */
        printf("%d, read_file failed!\n", __LINE__);
        return result;
    }

    result = (td_u32)strtol(char_tmp, NULL, 10); /* 10 base */
    return result;
}

static td_u32 ot_get_still_maxpacket(td_u32 dev_no)
{
    td_u32 result = 32768;
    td_char char_tmp[128] = {0};
    td_char file_path[128] = {0};
    (td_void)snprintf_truncated_s(file_path, sizeof(file_path),
        "/sys/kernel/config/usb_gadget/camera/functions/uvc.usb%u/still_maxpacket", dev_no);

    if (read_file(file_path, GET_MAX_PACKET_SIZE_PATH, char_tmp, 128) != 0) { /* 128 bytes used for char_tmp */
        printf("%d, read_file failed!\n", __LINE__);
        return result;
    }

    result = (td_u32)strtol(char_tmp, NULL, 10); /* 10 base */
    return result;
}
static td_void ot_uvc_handle_streaming_still_control(struct ot_uvc_dev *uvc_dev,
    struct ot_uvc_streaming_still_control *u_still_ctrl, td_u8 i_frame, td_u8 i_format)
{
    const struct ot_uvc_format_info *fmt_info = NULL;
    const struct ot_uvc_frame_info *frm_info = NULL;
    td_u8 n_frm = 0;
    td_u32 max_video_frame_size = 0;

    if (i_format >= array_size(ot_fmt)) {
        i_format = 0;
    }

    fmt_info = &ot_fmt[i_format];

    while (fmt_info->frames[n_frm].width != 0) {
        ++n_frm;
    }

    if (i_frame >= n_frm) {
        i_frame = 0;
    }

    frm_info = &fmt_info->frames[i_frame];

    (td_void)memset_s(u_still_ctrl, sizeof(*u_still_ctrl), 0, sizeof(*u_still_ctrl));

    u_still_ctrl->b_format_index = i_format + 1;               /* Yuv: 1, Mjpeg: 2. */
    u_still_ctrl->b_frame_index = i_frame + 1;                 /* 360p: 1 720p: 2. */
    u_still_ctrl->b_compression_index = 0;

    ot_uvc_get_buf_size(uvc_dev, frm_info->width,  frm_info->height, fmt_info->fcc, &max_video_frame_size);
    (td_void)memcpy_s(&u_still_ctrl->dw_max_video_frame_size, sizeof(td_u32), &max_video_frame_size, sizeof(td_u32));

    if (uvc_dev->dw_bulk != 0) {
        u_still_ctrl->dw_max_payload_transfer_size = uvc_dev->dw_bulk_size; /* This should be filled by the driver. */
    } else {
        u_still_ctrl->dw_max_payload_transfer_size = get_max_payload_transfer_size(frm_info);
    }

    if (ot_get_still_capture_method(uvc_dev->dev_no) == UVC_STILL_IMAGE_METHOD3) {
        u_still_ctrl->dw_max_payload_transfer_size = ot_get_still_maxpacket(uvc_dev->dev_no);
    }
}

static td_void ot_uvc_handle_streaming_still_request(struct ot_uvc_dev *uvc_dev, td_u8 req, td_u8 cs,
    struct uvc_request_data *u_req_data)
{
    struct ot_uvc_streaming_still_control u_still_ctrl = *(struct ot_uvc_streaming_still_control *)(u_req_data->data);
    struct ot_uvc_streaming_still_control *still_ctrl;

    u_req_data->length = 0x0B;
    const struct ot_uvc_format_info *format = &ot_fmt[uvc_dev->probe_still.b_format_index - 1];
    const struct ot_uvc_frame_info *frame = &format->frames[uvc_dev->probe_still.b_frame_index - 1];

    switch (req) {
        case OT_UVC_SET_CUR:
            uvc_dev->i_control = cs;
            u_still_ctrl.dw_max_payload_transfer_size =
                uvc_dev->dw_bulk != 0 ? uvc_dev->dw_bulk_size : get_max_payload_transfer_size(frame);
            if (ot_get_still_capture_method(uvc_dev->dev_no) == UVC_STILL_IMAGE_METHOD3) {
                u_still_ctrl.dw_max_payload_transfer_size = ot_get_still_maxpacket(uvc_dev->dev_no);
            }
            break;
        case OT_UVC_GET_CUR:
            still_ctrl = (cs == OT_UVC_VS_STILL_PROBE_CONTROL) ? &uvc_dev->probe_still : &uvc_dev->commit_still;
            (td_void)memcpy_s(&u_still_ctrl, sizeof(u_still_ctrl), still_ctrl, sizeof(u_still_ctrl));
            u_still_ctrl.dw_max_payload_transfer_size =
                uvc_dev->dw_bulk != 0 ? uvc_dev->dw_bulk_size : get_max_payload_transfer_size(frame);
            if (ot_get_still_capture_method(uvc_dev->dev_no) == UVC_STILL_IMAGE_METHOD3) {
                u_still_ctrl.dw_max_payload_transfer_size = ot_get_still_maxpacket(uvc_dev->dev_no);
            }
            break;
        case OT_UVC_GET_MIN:
        case OT_UVC_GET_MAX:
        case OT_UVC_GET_DEF:
            ot_uvc_handle_streaming_still_control(uvc_dev, &u_still_ctrl, uvc_dev->probe_still.b_frame_index - 1, \
                uvc_dev->probe_still.b_format_index - 1);
            break;
        case OT_UVC_GET_RES:
            (td_void)memset_s(u_req_data->data, sizeof(u_req_data->data), 0, sizeof(u_req_data->data));
            return;
        case OT_UVC_GET_LEN:
            u_req_data->data[0] = 0x00;
            u_req_data->data[1] = 0x0B;
            u_req_data->length = 0x2;
            return;
        case OT_UVC_GET_INFO:
            u_req_data->data[0] = 0x03;
            u_req_data->length = 1;
            return;
        default:
            return;
    }

    errno_t ret = memcpy_s(u_req_data->data, sizeof(u_req_data->data), &u_still_ctrl, sizeof(u_still_ctrl));
    if (ret != EOK) {
        ERR_INFO("copy back u_still_ctrl to req_data fail %d\n", ret);
    }
}

static struct ot_uvc_streaming_still_control *get_streaming_still_control(td_u32 control_type,
    struct ot_uvc_dev *uvc_dev, const struct uvc_request_data *u_req_data)
{
    switch (control_type) {
        case OT_UVC_VS_STILL_PROBE_CONTROL:
            return &uvc_dev->probe_still;
        case OT_UVC_VS_STILL_COMMIT_CONTROL:
            return &uvc_dev->commit_still;
        case OT_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL:
            return &uvc_dev->commit_still;
        default:
            return NULL;
    }
}

static td_void do_ot_uvc_still_commit_trigger_event(struct ot_uvc_dev *uvc_dev,
    struct ot_uvc_streaming_still_control *u_still_target, const struct ot_uvc_frame_info *frm_info, td_u32 fmt)
{
    int ret;
    struct video_framebuffer *still_image = &uvc_dev->still_image;

    if (uvc_dev->i_control == OT_UVC_VS_STILL_COMMIT_CONTROL) {
        still_image->fmt.height = frm_info->height;
        still_image->fmt.width = frm_info->width;
        still_image->fmt.pixelformat = fmt;
        still_image->fmt.sizeimage = u_still_target->dw_max_video_frame_size;
    } else if (uvc_dev->i_control == OT_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL) {
        ret = sample_get_still_image(still_image);
        if (ret < 0) {
            printf("[sample] get still_image failed, ret: %d\n", ret);
        }

        if (still_image->base == NULL) {
            printf("[sample] get still image failed, addr is NULL!\n");
        }

        ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_SET_FRAME_BUF, still_image);
        if (ret < 0) {
            printf("[sample] VIDEO_IOCTL_SET_FRAME_BUF failed: %d\n", errno);
        }

        free(still_image->base);
        still_image->base = NULL;
    }
}

static td_void do_ot_uvc_streaming_still_data_event(struct ot_uvc_dev *uvc_dev, struct uvc_request_data *u_req_data)
{
    struct ot_uvc_streaming_still_control *u_still_target = NULL;
    struct ot_uvc_streaming_still_control u_still_ctrl;
    const struct ot_uvc_format_info *fmt_info = NULL;
    const struct ot_uvc_frame_info *frm_info = NULL;
    td_u32 i_fmt, i_frm;
    td_u32 n_frm;

    if ((u_still_target = get_streaming_still_control(uvc_dev->i_control, uvc_dev, u_req_data)) == NULL) {
        return;
    }

    td_s32 ret = memcpy_s(&u_still_ctrl, sizeof(u_still_ctrl), u_req_data->data, sizeof(u_still_ctrl));
    if (ret != EOK) {
        ERR_INFO("memcpy_s u_still_ctrl fail, ret %d\n", ret);
        return;
    }

    i_fmt = clamp((td_u32)u_still_ctrl.b_format_index, 1U, (td_u32)array_size(ot_fmt));
    fmt_info = &ot_fmt[i_fmt - 1];
    n_frm = 0;

    while (fmt_info->frames[n_frm].width != 0) {
        ++n_frm;
    }

    i_frm = clamp((td_u32)u_still_ctrl.b_frame_index, 1U, n_frm);
    frm_info = &fmt_info->frames[i_frm - 1];

    u_still_target->b_format_index = (td_u8)i_fmt;
    u_still_target->b_frame_index = (td_u8)i_frm;

    switch (fmt_info->fcc) {
        case VIDEO_IMG_FORMAT_YUYV:
            u_still_target->dw_max_video_frame_size = frm_info->width * frm_info->height * 2; /* 2 bytes a pixel */
            break;
        case VIDEO_IMG_FORMAT_NV12:
        case VIDEO_IMG_FORMAT_NV21:
        case VIDEO_IMG_FORMAT_YUV420:
            u_still_target->dw_max_video_frame_size =
                frm_info->width * frm_info->height * 3 / 2; /* 3 / 2, 1.5 bytes a pixel */
            break;
        case VIDEO_IMG_FORMAT_MJPEG:
            if (uvc_dev->dw_img_size == 0) {
                printf("WARNING: MJPEG requested and no image loaded.\n");
            }
            u_still_target->dw_max_video_frame_size = uvc_dev->dw_img_size;
            break;
        default:
            printf("WARNING: wrong image format\n");
            break;
    }

    do_ot_uvc_still_commit_trigger_event(uvc_dev, u_still_target, frm_info, fmt_info->fcc);
}

#ifdef UVC_STILL_BUTTON_TEST
int button_pressed()
{
    struct ot_uvc_dev *uvc_dev = g_ot_cd[0];
    struct video_framebuffer tmp;
    int ret;

    if (uvc_dev->i_streaming == 0) {
        return 0;
    }

    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_BUTTON, (void*)&tmp);
    if (ret < 0) {
        printf("VIDEO_IOCTL_BUTTON ret:%d errno:%d\n", ret, errno);
    }
    return ret;
}
#endif
#endif

static td_u32 ot_get_max_payload_transfer_size_for_bulk(td_u32 dev_no)
{
    td_u32 result = 1024;
    td_char char_tmp[128] = {0};
    td_char file_path[128] = {0};
    (td_void)snprintf_truncated_s(file_path, sizeof(file_path),
        "/sys/kernel/config/usb_gadget/camera/functions/uvc.usb%u/streaming_maxpacket", dev_no);

    if (read_file(file_path, GET_MAX_PACKET_SIZE_PATH, char_tmp, 128) != 0) { /* 128 bytes used for char_tmp */
        printf("%d, read_file failed!\n", __LINE__);
        return result;
    }
    printf("%s, %s \n", __func__, char_tmp);
    result = (td_u32)strtol(char_tmp, NULL, 10); /* 10 base */
    return result;
}

static td_void ot_uvc_handle_streaming_control(struct ot_uvc_dev *uvc_dev, struct ot_uvc_streaming_control *u_str_ctrl,
    td_u8 i_frame, td_u8 i_format)
{
    const struct ot_uvc_format_info *fmt_info = NULL;
    const struct ot_uvc_frame_info *frm_info = NULL;
    td_u8 n_frm = 0;
    td_u32 max_video_frame_size = 0;

    if (i_format >= array_size(ot_fmt)) {
        i_format = 0;
    }

    debug("i_format = %d\n", i_format);
    fmt_info = &ot_fmt[i_format];

    while (fmt_info->frames[n_frm].width != 0) {
        ++n_frm;
    }

    if (i_frame >= n_frm) {
        i_frame = 0;
    }

    frm_info = &fmt_info->frames[i_frame];

    (td_void)memset_s(u_str_ctrl, sizeof(*u_str_ctrl), 0, sizeof(*u_str_ctrl));

    u_str_ctrl->bm_hint = 1;
    u_str_ctrl->b_format_index = i_format + 1;               /* Yuv: 1, Mjpeg: 2. */
    u_str_ctrl->b_frame_index = i_frame + 1;                 /* 360p: 1 720p: 2. */
    u_str_ctrl->dw_frame_interval = frm_info->intervals[0]; /* Corresponding to the number of frame rate. */

    ot_uvc_get_buf_size(uvc_dev, frm_info->width, frm_info->height, fmt_info->fcc, &max_video_frame_size);
    (td_void)memcpy_s(&u_str_ctrl->dw_max_video_frame_size, sizeof(td_u32), &max_video_frame_size, sizeof(td_u32));

    if (uvc_dev->dw_bulk != 0) {
        u_str_ctrl->dw_max_payload_transfer_size = uvc_dev->dw_bulk_size; /* This should be filled by the driver. */
    } else {
        u_str_ctrl->dw_max_payload_transfer_size = get_max_payload_transfer_size(frm_info);
    }
    u_str_ctrl->bm_framing_info = 3; /* 3 format mode */
    u_str_ctrl->b_prefered_version = 1;
    u_str_ctrl->b_max_version = 1;

    u_str_ctrl->dw_clock_frequency = 48000000; // 48000000 Hz
    u_str_ctrl->b_usage = 17; // 17 file-storage mode
    u_str_ctrl->b_msettings = 0x20;
    u_str_ctrl->b_max_number_of_ref_frames_plus1 = 1;
    u_str_ctrl->b_rate_control_modes = 0x22;
}

static td_void ot_uvc_handle_standard_request(const struct ot_uvc_dev *uvc_dev,
    const struct usb_ctrlrequest *u_ctrl_req, const struct uvc_request_data *u_req_data)
{
    debug("camera standard request\n");
}


static void ot_uvc_eve_undef_control(struct ot_uvc_dev *uvc_dev, td_u8 cs, struct uvc_request_data *u_req_data)
{
    switch (cs) {
        case OT_UVC_VC_REQUEST_ERROR_CODE_CONTROL:
            u_req_data->length = uvc_dev->request_error_code.length;
            u_req_data->data[0] = uvc_dev->request_error_code.data[0];
            break;
        default:
            uvc_dev->request_error_code.length = 1;
            uvc_dev->request_error_code.data[0] = 0x06;
            break;
    }
}

static td_void ot_uvc_handle_control_request(struct ot_uvc_dev *uvc_dev, td_u8 req, td_u8 unit_id, td_u8 cs,
    struct uvc_request_data *u_req_data)
{
    switch (unit_id) {
        case OT_UVC_ID_UNDEFINED:
            ot_uvc_eve_undef_control(uvc_dev, cs, u_req_data);
            break;
        case OT_UVC_CAMERA_TERMINAL_ID:
            ot_stream_event_ct_control(uvc_dev, req, unit_id, cs, u_req_data);
            break;
        case OT_UVC_PROCESSING_UNIT_ID:
            ot_stream_event_pu_control(uvc_dev, req, unit_id, cs, u_req_data);
            break;
        case OT_UVC_ENCODING_UNIT_ID:
            ot_stream_event_eu_control(uvc_dev, req, unit_id, cs, u_req_data);
            break;
        case OT_UVC_EXTENSION_UNIT_ID:
            ot_stream_event_xu_control(uvc_dev, req, unit_id, cs, u_req_data);
            break;
        default:
            uvc_dev->request_error_code.length = 1;
            uvc_dev->request_error_code.data[0] = 0x06;
    }
}

static td_u16 ot_uvc_get_version(td_u32 dev_no)
{
    td_u16 result = 0x0110;
    td_char char_tmp[128] = {0};
    td_char file_path[128] = {0};
    (td_void)snprintf_truncated_s(file_path, sizeof(file_path),
        "/sys/kernel/config/usb_gadget/camera/functions/uvc.usb%u/control/header/h/bcdUVC", dev_no);

    if (read_file(file_path, GET_MAX_PACKET_SIZE_PATH, char_tmp, 128) != 0) { /* 128 bytes used for char_tmp */
        printf("%d, read_file failed!\n", __LINE__);
        return result;
    }
    result = (td_u16)strtol(char_tmp, NULL, 10); /* 10 base */
    return result;
}

static td_s32 proc_still_image_request(struct ot_uvc_dev *uvc_dev, td_u8 req,
    td_u8 cs, struct uvc_request_data *u_req_data)
{
#ifdef UVC_STILL_IMAGE
    if (is_still_image(cs)) {
        ot_uvc_handle_streaming_still_request(uvc_dev, req, cs, u_req_data);
        return TD_SUCCESS;
    }
#endif

    return TD_FAILURE;
}

static td_void ot_uvc_handle_streaming_request(struct ot_uvc_dev *uvc_dev,
    td_u8 req, td_u8 cs, struct uvc_request_data *u_req_data)
{
    struct ot_uvc_streaming_control u_str_ctrl;
    struct ot_uvc_streaming_control *str_ctl;

    if (proc_still_image_request(uvc_dev, req, cs, u_req_data) == TD_SUCCESS) {
        return;
    }

    if ((cs != OT_UVC_VS_PROBE_CONTROL) && (cs != OT_UVC_VS_COMMIT_CONTROL)) {
        return;
    }

    u_str_ctrl = *(struct ot_uvc_streaming_control *)(u_req_data->data);
    u_req_data->length = (ot_uvc_get_version(uvc_dev->dev_no) == 0x0150) ? 0x30 : 0x22;
    const struct ot_uvc_format_info *format = &ot_fmt[uvc_dev->probe.b_format_index - 1];
    const struct ot_uvc_frame_info *frame = &format->frames[uvc_dev->probe.b_frame_index - 1];
    td_u32 max_payload_size = uvc_dev->dw_bulk != 0 ? uvc_dev->dw_bulk_size : get_max_payload_transfer_size(frame);
    switch (req) {
        case OT_UVC_SET_CUR:
            uvc_dev->i_control = cs;
            u_str_ctrl.dw_max_payload_transfer_size = max_payload_size;
            break;
        case OT_UVC_GET_CUR:
            str_ctl = (cs == OT_UVC_VS_PROBE_CONTROL) ? &uvc_dev->probe : &uvc_dev->commit;
            (td_void)memcpy_s(&u_str_ctrl, sizeof(u_str_ctrl), str_ctl, sizeof(u_str_ctrl));
            u_str_ctrl.dw_max_payload_transfer_size = max_payload_size;
            break;
        case OT_UVC_GET_MIN:
        case OT_UVC_GET_MAX:
        case OT_UVC_GET_DEF:
            ot_uvc_handle_streaming_control(uvc_dev, &u_str_ctrl, uvc_dev->probe.b_frame_index - 1, \
                uvc_dev->probe.b_format_index - 1);
            break;
        case OT_UVC_GET_RES:
            (td_void)memset_s(u_req_data->data, sizeof(u_req_data->data), 0, sizeof(u_req_data->data));
            return;
        case OT_UVC_GET_LEN:
            u_req_data->data[0] = 0x00;
            u_req_data->data[1] = (ot_uvc_get_version(uvc_dev->dev_no) == 0x0150) ? 0x30 : 0x22;
            u_req_data->length = 0x2;
            return;
        case OT_UVC_GET_INFO:
            u_req_data->data[0] = 0x03;
            u_req_data->length = 1;
            return;
        default:
            return;
    }

    errno_t ret = memcpy_s(u_req_data->data, sizeof(u_req_data->data), &u_str_ctrl, sizeof(u_str_ctrl));
    if (ret != EOK) {
        ERR_INFO("copy back u_str_ctrl to req_data fail %d\n", ret);
    }
}

static td_void set_probe_status(struct ot_uvc_dev *uvc_dev, td_s32 cs, td_s32 req)
{
    if (cs == 0x01) {
        switch (req) {
            case 0x01:
                uvc_dev->probe_status.b_set = 1;
                break;
            case 0x81:
                uvc_dev->probe_status.b_get = 1;
                break;
            case 0x82:
                uvc_dev->probe_status.b_min = 1;
                break;
            case 0x83:
                uvc_dev->probe_status.b_max = 1;
                break;
            case 0x84:
                break;
            case 0x85:
                break;
            case 0x86:
                break;
            default:
                break;
        }
    }
}

static td_void ot_uvc_handle_class_request(struct ot_uvc_dev *uvc_dev, struct usb_ctrlrequest *u_ctrl_req,
    struct uvc_request_data *u_req_data)
{
    const td_bool probe_status = TD_TRUE;

    if (probe_status) {
        td_u8 recip_type = u_ctrl_req->bRequestType & USB_RECIP_MASK;
        switch (recip_type) {
            case USB_RECIP_INTERFACE:
                debug("Request type: USB_RECIP_INTERFACE\n");
                debug("Request: 0x%02x(%s)\n", u_ctrl_req->bRequest, (td_char *)get_code(u_ctrl_req->bRequest));
                debug("Interface No.  %d\n", (u_ctrl_req->wIndex & 0xff));
                debug("Entity ID: %d\n", ((u_ctrl_req->wIndex & 0xff00) >> RIGHT_SHIFT_8BIT));
                set_probe_status(uvc_dev, (u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT), u_ctrl_req->bRequest);
                break;
            case USB_RECIP_DEVICE:
                debug("request type :DEVICE\n");
                break;
            case USB_RECIP_ENDPOINT:
                debug("request type :ENDPOINT\n");
                break;
            case USB_RECIP_OTHER:
                debug("request type :OTHER\n");
                break;
            default:
                break;
        }
    }

    if ((u_ctrl_req->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE) {
        return;
    }

    uvc_dev->i_control = (u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT);
    uvc_dev->i_unit_id = ((u_ctrl_req->wIndex & 0xff00) >> RIGHT_SHIFT_8BIT);
    uvc_dev->i_intf_id = (u_ctrl_req->wIndex & 0xff);

    switch (u_ctrl_req->wIndex & 0xff) {
        case OT_UVC_INTF_CONTROL:
        case OT_UVC_INTF_CONTROL2:
            debug("Control Selector ID: 0x%02x\n", (u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT));
            ot_uvc_handle_control_request(uvc_dev, u_ctrl_req->bRequest, u_ctrl_req->wIndex >> RIGHT_SHIFT_8BIT,
                u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT, u_req_data);
            break;
        case OT_UVC_INTF_STREAMING:
        case OT_UVC_INTF_STREAMING2:
            debug("Control Selector ID: 0x%02x(%s)\n", (u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT),
                    (td_char *)get_intf_cs_string((u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT)));
            ot_uvc_handle_streaming_request(uvc_dev, u_ctrl_req->bRequest, u_ctrl_req->wValue >> RIGHT_SHIFT_8BIT,
                u_req_data);
            break;
        default:
            break;
    }
}

static td_void do_ot_uvc_setup_event(struct ot_uvc_dev *uvc_dev, struct usb_ctrlrequest *u_ctrl_req,
    struct uvc_request_data *u_req_data)
{
    uvc_dev->i_control = 0;
    uvc_dev->i_unit_id = 0;
    uvc_dev->i_intf_id = 0;

    switch (u_ctrl_req->bRequestType & USB_TYPE_MASK) {
        case USB_TYPE_STANDARD:
            ot_uvc_handle_standard_request(uvc_dev, u_ctrl_req, u_req_data);
            break;
        case USB_TYPE_CLASS:
            ot_uvc_handle_class_request(uvc_dev, u_ctrl_req, u_req_data);
            break;
        default:
            break;
    }
}

static td_void handle_control_interface_data(struct ot_uvc_dev *uvc_dev, struct uvc_request_data *u_req_data)
{
    switch (uvc_dev->i_unit_id) {
        case OT_UVC_CAMERA_TERMINAL_ID:
            ot_stream_event_ct_data(uvc_dev, uvc_dev->i_unit_id, uvc_dev->i_control, u_req_data);
            break;
        case OT_UVC_PROCESSING_UNIT_ID:
            ot_stream_event_pu_data(uvc_dev, uvc_dev->i_unit_id, uvc_dev->i_control, u_req_data);
            break;
        case OT_UVC_ENCODING_UNIT_ID:
            ot_stream_event_eu_data(uvc_dev, uvc_dev->i_unit_id, uvc_dev->i_control, u_req_data);
            break;
        case OT_UVC_EXTENSION_UNIT_ID:
            ot_stream_event_xu_data(uvc_dev, uvc_dev->i_unit_id, uvc_dev->i_control, u_req_data);
            break;
        default:
            break;
    }
}

static td_u32 clamp(td_u32 val, td_u32 min, td_u32 max)
{
    td_u32 res = val;
    if (res < min) {
        return min;
    }
    if (res > max) {
        return max;
    }
    return val;
}

static void compute_video_frame_size(td_u32 img_fmt, const struct ot_uvc_dev *uvc_dev,
    struct ot_uvc_streaming_control *u_str_target, const struct ot_uvc_frame_info *frm_info)
{
    switch (img_fmt) {
        case VIDEO_IMG_FORMAT_YUYV:
            u_str_target->dw_max_video_frame_size = frm_info->width * frm_info->height * 2; /* 2 bytes a pixel */
            break;
        case VIDEO_IMG_FORMAT_NV12:
        case VIDEO_IMG_FORMAT_NV21:
        case VIDEO_IMG_FORMAT_YUV420:
            u_str_target->dw_max_video_frame_size =
                frm_info->width * frm_info->height * 3 / 2; /* 3 / 2, 1.5 bytes a pixel */
            break;
        case VIDEO_IMG_FORMAT_MJPEG:
        case VIDEO_IMG_FORMAT_H264:
        case VIDEO_IMG_FORMAT_H265:
            if (uvc_dev->dw_img_size == 0) {
                debug("WARNING: MJPEG requested and no image loaded.\n");
            }

            u_str_target->dw_max_video_frame_size = uvc_dev->dw_img_size;
            break;
        default:
            debug("WARNING: wrong image format\n");
            break;
    }
}

static struct ot_uvc_streaming_control *get_streaming_control(td_u32 control_type, struct ot_uvc_dev *uvc_dev,
    const struct uvc_request_data *u_req_data)
{
    switch (control_type) {
        case OT_UVC_VS_PROBE_CONTROL:
            debug("setting probe control, length = %d\n", u_req_data->length);
            return &uvc_dev->probe;
        case OT_UVC_VS_COMMIT_CONTROL:
            debug("setting commit control, length = %d\n", u_req_data->length);
            return &uvc_dev->commit;
        default:
            debug("setting unknown control, length = %d\n", u_req_data->length);
            return NULL;
    }
}

static td_void do_ot_uvc_vs_commit(struct ot_uvc_dev *uvc_dev, const struct ot_uvc_streaming_control *u_str_target,
    const struct ot_uvc_format_info *fmt_info, const struct ot_uvc_frame_info *frm_info)
{
    uvc_dev->dw_fcc = fmt_info->fcc;
    uvc_dev->dw_width = frm_info->width;
    uvc_dev->dw_height = frm_info->height;
    uvc_dev->i_fps = FRAME_INTERVAL_CALC_100NS / u_str_target->dw_frame_interval;

    printf("set format=%s  %ux%u\n", image_fmt_to_string(uvc_dev->dw_fcc), uvc_dev->dw_width, uvc_dev->dw_height);

    ot_uvc_video_set_fmt(uvc_dev);

    if (uvc_dev->dw_bulk != 0) {
        ot_uvc_video_disable(uvc_dev);
        ot_uvc_video_enable(uvc_dev);
    }
    (td_void)memset_s(&uvc_dev->probe_status, sizeof(uvc_dev->probe_status), 0, sizeof(uvc_dev->probe_status));
}

static td_void do_ot_uvc_data_event(struct ot_uvc_dev *uvc_dev, struct uvc_request_data *u_req_data)
{
    struct ot_uvc_streaming_control *u_str_target = NULL;
    struct ot_uvc_streaming_control u_str_ctrl;
    const struct ot_uvc_format_info *fmt_info = NULL;
    const struct ot_uvc_frame_info *frm_info = NULL;
    const td_u32 *intval = NULL;
    td_u32 i_fmt, i_frm;
    td_u32 n_frm = 0;

    if ((uvc_dev->i_unit_id != 0) &&
        (uvc_dev->i_intf_id == OT_UVC_INTF_CONTROL || uvc_dev->i_intf_id == OT_UVC_INTF_CONTROL2)) {
        return handle_control_interface_data(uvc_dev, u_req_data);
    }

#ifdef UVC_STILL_IMAGE
    if (is_still_image(uvc_dev->i_control)) {
        return do_ot_uvc_streaming_still_data_event(uvc_dev, u_req_data);
    }
#endif

    if ((u_str_target = get_streaming_control(uvc_dev->i_control, uvc_dev, u_req_data)) == NULL) {
        return;
    }

    td_s32 ret = memcpy_s(&u_str_ctrl, sizeof(u_str_ctrl), u_req_data->data, sizeof(u_str_ctrl));
    if (ret != EOK) {
        ERR_INFO("memcpy_s u_str_ctrl fail, ret %d\n", ret);
        return;
    }

    i_fmt = clamp((td_u32)u_str_ctrl.b_format_index, 1U, (td_u32)array_size(ot_fmt));

    fmt_info = &ot_fmt[i_fmt - 1];

    while (fmt_info->frames[n_frm].width != 0) {
        ++n_frm;
    }

    i_frm = clamp((td_u32)u_str_ctrl.b_frame_index, 1U, n_frm);
    frm_info = &fmt_info->frames[i_frm - 1];
    intval = frm_info->intervals;

    while ((intval[0] < u_str_ctrl.dw_frame_interval) && intval[1] != 0) {
        ++intval;
    }

    u_str_target->b_format_index = (td_u8)i_fmt;
    u_str_target->b_frame_index = (td_u8)i_frm;

    compute_video_frame_size(fmt_info->fcc, uvc_dev, u_str_target, frm_info);

    u_str_target->dw_frame_interval = *intval;

    if (uvc_dev->i_control == OT_UVC_VS_COMMIT_CONTROL) {
        do_ot_uvc_vs_commit(uvc_dev, u_str_target, fmt_info, frm_info);
    }
}

static td_void do_ot_uvc_event(struct ot_uvc_dev *uvc_dev)
{
    struct video_event v_event;
    struct ot_uvc_event *ot_eve = (struct ot_uvc_event *)(td_void *)&v_event.u.a_data;
    struct uvc_request_data u_req_data;
    td_s32 ret;

    debug("#############do_ot_uvc_event()#############\n");

    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DQUEUE_EVENT, &v_event);
    if (ret < 0) {
        ERR_INFO("VIDEO_IOCTL_DQUEUE_EVENT failed: %d\n", errno);
        return;
    }

    (td_void)memset_s(&u_req_data, sizeof(u_req_data), 0, sizeof(u_req_data));
    u_req_data.length = 32; /* 32 byte data length */

    switch (v_event.dw_type) {
        case OT_UVC_EVE_CON:
            debug("handle connect event\n");
            ot_uvc_handle_streaming_control(uvc_dev, &uvc_dev->probe, 0, 0);
            ot_uvc_handle_streaming_control(uvc_dev, &uvc_dev->commit, 0, 0);
#ifdef UVC_STILL_IMAGE
            ot_uvc_handle_streaming_still_control(uvc_dev, &uvc_dev->probe_still, 0, 0);
            ot_uvc_handle_streaming_still_control(uvc_dev, &uvc_dev->commit_still, 0, 0);
#endif
            return;
        case OT_UVC_EVE_DISCON:
            if (uvc_dev->dw_bulk == 0) {
                ot_uvc_video_disable(uvc_dev);
            }
            return;
        case OT_UVC_EVE_SETTING:
            do_ot_uvc_setup_event(uvc_dev, &ot_eve->req, &u_req_data);
            break;
        case OT_UVC_EVE_DATA:
            do_ot_uvc_data_event(uvc_dev, &ot_eve->data);
            return;
        case OT_UVC_EVE_STRON:
            if (uvc_dev->dw_bulk != 1) {
                ot_uvc_video_enable(uvc_dev);
            }
            return;
        case OT_UVC_EVE_STROFF:
            ot_uvc_video_disable(uvc_dev);
            return;
        default:
            break;
    }

    ret = ioctl(uvc_dev->i_fd, OT_UVC_IOC_SEND_RESPONSE, &u_req_data);
    if (ret < 0) {
        ERR_INFO("OT_UVC_IOC_S_EVENT failed: %d\n", errno);
    }
}

static td_void ot_uvc_event_register(struct ot_uvc_dev *uvc_dev)
{
    struct video_event_descriptor v_event_desc;
    td_s32 ret;

    (td_void)memset_s(&v_event_desc, sizeof(v_event_desc), 0, sizeof(v_event_desc));

    v_event_desc.dw_type = OT_UVC_EVE_CON;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DESC_EVENT, &v_event_desc);
    if (ret < 0) {
        ERR_INFO("Connect event failed: %d\n", errno);
    }

    v_event_desc.dw_type = OT_UVC_EVE_DISCON;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DESC_EVENT, &v_event_desc);
    if (ret < 0) {
        ERR_INFO("Disconnect event failed: %d\n", errno);
    }

    v_event_desc.dw_type = OT_UVC_EVE_SETTING;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DESC_EVENT, &v_event_desc);
    if (ret < 0) {
        ERR_INFO("Setup event failed: %d\n", errno);
    }

    v_event_desc.dw_type = OT_UVC_EVE_DATA;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DESC_EVENT, &v_event_desc);
    if (ret < 0) {
        ERR_INFO("Data event failed: %d\n", errno);
    }

    v_event_desc.dw_type = OT_UVC_EVE_STRON;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DESC_EVENT, &v_event_desc);
    if (ret < 0) {
        ERR_INFO("StreamOn event failed: %d\n", errno);
    }

    v_event_desc.dw_type = OT_UVC_EVE_STROFF;
    ret = ioctl(uvc_dev->i_fd, VIDEO_IOCTL_DESC_EVENT, &v_event_desc);
    if (ret < 0) {
        ERR_INFO("StreamOff event failed: %d\n", errno);
    }
}

static td_void get_bulk_mode(td_u32 *bulk_mode, td_u32 dev_no)
{
    if (bulk_mode == NULL) {
        return;
    }

    td_char char_tmp[128] = {0};
    td_char file_path[128] = {0};
    *bulk_mode = 0;

    (td_void)snprintf_truncated_s(file_path, sizeof(file_path),
        "/sys/kernel/config/usb_gadget/camera/functions/uvc.usb%u/streaming_transfer", dev_no);

    if (read_file(file_path, GET_MAX_PACKET_SIZE_PATH, char_tmp, 128) != 0) { /* 128 bytes used for char_tmp */
        printf("%d, read_file failed!\n", __LINE__);
        return;
    }
    printf("%s, %s \n", __func__, char_tmp);
    if (strcmp(char_tmp, "bulk") == 0) {
        *bulk_mode = 1;
    }
    return;
}

td_s32 open_uvc_device(const td_char *dev_path, td_u32 dev_no)
{
    struct ot_uvc_dev *uvc_dev;

    uvc_dev = ot_uvc_init(dev_path, dev_no);
    if (uvc_dev == 0) {
        return -1;
    }

    uvc_dev->uvc_cache = uvc_cache_get(dev_no);

    uvc_dev->dw_img_size = MAX_PAYLOAD_IMAGE_SIZE; /* for VENC stream max size */

    get_bulk_mode(&uvc_dev->dw_bulk, dev_no);
    if (uvc_dev->dw_bulk == 1) {
        uvc_dev->dw_bulk_size = ot_get_max_payload_transfer_size_for_bulk(uvc_dev->dev_no);
    }

    debug("set imagesize = %d, set bulkmode =%d, set bulksize = %d\n", uvc_dev->dw_img_size, uvc_dev->dw_bulk,
        uvc_dev->dw_bulk_size);

    ot_uvc_event_register(uvc_dev);
    g_ot_cd[dev_no] = uvc_dev;

    return 0;
}

td_s32 close_uvc_device(td_u32 dev_no)
{
    if (g_ot_cd[dev_no] != 0) {
        ot_uvc_video_disable(g_ot_cd[dev_no]);
        close(g_ot_cd[dev_no]->i_fd);
        free(g_ot_cd[dev_no]);
    }

    g_ot_cd[dev_no] = NULL;
    return 0;
}

td_s32 run_uvc_data(td_u32 dev_no)
{
    fd_set w_fds;
    td_s32 ret;
    struct timeval t_val;

    if (g_ot_cd[dev_no] == NULL) {
        return -1;
    }

    t_val.tv_sec = 0;
    t_val.tv_usec = SELECT_DELAY_USECS;

    FD_ZERO(&w_fds);

    if (g_ot_cd[dev_no]->i_streaming == 1) {
        FD_SET(g_ot_cd[dev_no]->i_fd, &w_fds);
    }

    ret = select(g_ot_cd[dev_no]->i_fd + 1, NULL, &w_fds, NULL, &t_val);
    if (ret > 0) {
        if (FD_ISSET(g_ot_cd[dev_no]->i_fd, &w_fds)) {
            ot_uvc_video_process_user_ptr(g_ot_cd[dev_no]);
        }
    }

    return ret;
}

td_s32 run_uvc_device(td_u32 dev_no)
{
    fd_set e_fds;
    td_s32 ret;
    struct timeval t_val;

    if (g_ot_cd[dev_no] == NULL) {
        return -1;
    }

    t_val.tv_sec = 1;
    t_val.tv_usec = 0;

    FD_ZERO(&e_fds);
    FD_SET(g_ot_cd[dev_no]->i_fd, &e_fds);

    ret = select(g_ot_cd[dev_no]->i_fd + 1, NULL, NULL, &e_fds, &t_val);
    if (ret > 0) {
        if (FD_ISSET(g_ot_cd[dev_no]->i_fd, &e_fds)) {
            do_ot_uvc_event(g_ot_cd[dev_no]);
        }
    }

    return ret;
}
