/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "ff_vdec.h"

#include <sys/prctl.h>

#include <limits.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"

#include "ss_mpi_sys_mem.h"

#include "av_list.h"
#include "avplay_common.h"

#include "ff_vbbuf.h"

#include "ff_vsrc.h"
#include "sample_comm.h"

#define ADDR_ALIGIN 16
#define STRIDE_ALIGN 8

#define SEC_TO_USEC 1000000
#define SEC_UNIT 1000

// Y U V alpha
#define VIDEO_PLANE_SIZE      4
#define MAX_STREAM_CACHE_SIZE 3
#define YUV42X_DATA_INDEX_Y   0
#define YUV42X_DATA_INDEX_U   1
#define YUV42X_DATA_INDEX_V   2
#define YUV_DATA_NUM          3
#define YVU420_2_YUV420_HEIGHT 2
#define MMZ_FRAME_NUM         10

#define MAX_WAIT_VBBLK_TIMES   100
#define MAX_SLEEP_WHEN_NO_BUF  20000
#define MAX_SLEEP_WHEN_NO_DATA 30000

#define VIDEO_SOURCE_NAME "vdec_source"

#define MBUF_VIDEO_PAYLOAD 12

#define ERR_VDEC_NO_VBBLK       0x100
#define MAX_CACHE_FRAME_NUM     3
#define ERR_VDEC_FRAME_FULL     0x101
#define FF_VDEC_THREAD_PRIORITY 6
#define FF_VDEC_PRINTBUF_MAXLEN 1024

typedef struct {
    td_s32 line_sizes;
    td_u8 *adst_ptr;
} ff_vdec_frame_info_part;

typedef struct {
    ot_video_frame_info frame_data;
    td_bool readed;
    /* do not change */
    td_void *priv;
} ff_vdec_frame_info;

typedef struct {
    AVCodecContext *codec_ctx;
    pthread_t thread_id;
    td_bool started;
    td_bool running;
    pthread_mutex_t dec_lock;
    AVDictionary *dec_opts;
    td_u8 *uv_cache_buf;
    td_u32 uv_cache_size;
    ot_payload_type vdec_type;
    av_vdec_attr vdec_attr;

    td_u64 phy_addr[MMZ_FRAME_NUM];
    ff_vdec_source stream_source;
    td_s32 got_pic;
    AVFrame *stav_frame;

    av_list_instance *frame_wait_list;
} ff_vdec_ctx;

static td_s32 ff_vdecoder_write_stream(const ff_vdec_source *stream_source, const av_vdec_stream *stream_info)
{
    ot_mbuf_sliceinfo mbuf_slice;
    if (memset_s(&mbuf_slice, sizeof(mbuf_slice), 0x00, sizeof(mbuf_slice)) != EOK) {
        sample_print("set mbuf_slice failed \n");
        return TD_FAILURE;
    }

    mbuf_slice.is_key_frame = TD_TRUE;
    mbuf_slice.payload_type = MBUF_VIDEO_PAYLOAD;
    mbuf_slice.slice_type = SINGLE_FRAME_SLICE; /* 0x05-single frame data */
    mbuf_slice.tail_len = AV_INPUT_BUFFER_PADDING_SIZE;
    mbuf_slice.slice_cnt = ONE_SLICE;

    mbuf_slice.addr[0] = stream_info->addr;
    mbuf_slice.au32_block_len[0] = stream_info->len;
    mbuf_slice.block_count = 1;

    mbuf_slice.slice_len = stream_info->len;
    mbuf_slice.frame_len = stream_info->len;
    mbuf_slice.pts = stream_info->pts;

    td_s32 ret = ff_vdec_source_write_data((ff_vdec_source *)stream_source, &mbuf_slice);
    if (ret == ERR_MBUF_FULL) {
        ret = VDEC_ERR_STREAM_BUF_FULL;
    } else if (ret != TD_SUCCESS) {
        sample_print("ff_write_data failed, ret:%d \n", ret);
        ret = TD_FAILURE;
    }

    return ret;
}

static td_s32 ff_vdecoder_read_stream(ff_vdec_source *stream_source, av_vdec_stream *stream_info)
{
    ot_mbuf_sliceinfo mbuf_slice;
    if (memset_s(&mbuf_slice, sizeof(mbuf_slice), 0x00, sizeof(mbuf_slice)) != EOK) {
        sample_print("set mbuf_slice failed \n");
        return TD_FAILURE;
    }

    td_s32 ret = ff_vdec_source_acquire_data(stream_source, &mbuf_slice);
    if (ret != TD_SUCCESS) {
        // may be no data
        return ret;
    }

    if (mbuf_slice.block_count != 1) {
        AV_TRACE("video decoder read stream ret block count not 1\n");
        return TD_FAILURE;
    }

    stream_info->addr = mbuf_slice.addr[0];

    stream_info->len = mbuf_slice.au32_block_len[0];
    stream_info->pts = mbuf_slice.pts;

    return TD_SUCCESS;
}

static td_void ff_vdecoder_interleave_uv(AVFrame *frame, td_u32 u_v_height, td_u8 *dst_ptr)
{
    td_u8 *u_src_buf = frame->data[YUV42X_DATA_INDEX_U];
    td_u32 u_stride = frame->linesize[YUV42X_DATA_INDEX_U];
    td_u8 *v_src_buf = frame->data[YUV42X_DATA_INDEX_V];
    td_u32 v_stride = frame->linesize[YUV42X_DATA_INDEX_V];
    td_u8 *dst_buf = dst_ptr;

    for (td_u32 j = 0; j < u_v_height; j++) {
        for (td_u32 k = 0; k < u_stride; k++) {
            *dst_buf++ = v_src_buf[k];
            *dst_buf++ = u_src_buf[k];
        }
        u_src_buf += u_stride;
        v_src_buf += v_stride;
    }
}

static td_void ff_vdecoder_get_info(const AVFrame *frame, td_s32 *u_v_height, ot_pixel_format *pixfmt)
{
    if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
        /* convert from yuv420p to  YVU420SP(NV21) */
        *u_v_height = frame->height / YVU420_2_YUV420_HEIGHT;

#ifdef __HI3518EV300__
        *pixfmt = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
#else
        *pixfmt = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
#endif
    } else { /* yuv422p */
        /* convert from yuv422p to  YUV422SP(NV61)
        todo: ffmpeg do not support NV61 with swscale */
        *u_v_height = frame->height;
#ifdef __HI3518EV300__
        *pixfmt = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
#else
        *pixfmt = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
#endif
    }
}

static td_s32 ff_vdecoder_create_uv_cache(ff_vdec_ctx *vdec_ctx, td_u32 v_udata_size)
{
    if (v_udata_size == 0) {
        AV_TRACE("v_udata_size is 0\n");
        return TD_FAILURE;
    }
    // first time to create buffer
    if (vdec_ctx->uv_cache_buf == NULL) {
        vdec_ctx->uv_cache_buf = (td_u8 *)malloc(v_udata_size);
        if (vdec_ctx->uv_cache_buf == NULL) {
            AV_TRACE("malloc uv_cache_buf failed\n");
            return TD_FAILURE;
        }
        vdec_ctx->uv_cache_size = v_udata_size;
    }

    if (vdec_ctx->uv_cache_size < v_udata_size) {
        AV_TRACE("u_v_cache_size  smaller than v_udata_size \n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_void ff_vdecoder_set_vdec_frame_info(const ff_vdec_frame_info_part info[], td_u32 info_len,
    const vb_vdec_buffer *vb_buf, ff_vdec_frame_info *dst_frame)
{
    if (info_len <= (VIDEO_PLANE_SIZE - 1)) {
        return;
    }
    for (td_s32 i = 0; i < VIDEO_PLANE_SIZE - 1; i++) {
        dst_frame->frame_data.video_frame.virt_addr[i] = info[i].adst_ptr;
        dst_frame->frame_data.video_frame.phys_addr[i] = vb_buf->phy_addr + (info[i].adst_ptr -
            (td_u8 *)vb_buf->vir_addr);
        dst_frame->frame_data.video_frame.stride[i] = info[i].line_sizes;
    }
    dst_frame->frame_data.pool_id = vb_buf->pool_id;
    dst_frame->priv = (td_void *)vb_buf;
}

static td_s32 ff_vdecoder_create_vb_vdec(vb_vdec_buffer **vb_buf, td_u32 total_size)
{
    vb_vdec_buffer *vb = malloc(sizeof(vb_vdec_buffer));
    if (vb == NULL) {
        AV_TRACE("malloc vb_vdec_buffer failed\n");
        return TD_FAILURE;
    }
    
    if (ot_vdec_frame_buffer_alloc(total_size, vb) != TD_SUCCESS) {
        free(vb);
        return ERR_VDEC_NO_VBBLK;
    }
    *vb_buf = vb;
    return TD_SUCCESS;
}

static td_u32 dec_align(td_u32 size, td_u32 align)
{
    return (((size) / (align)) + 1) * (align);
}

static td_s32 ff_vdecoder_soft_yuv42_xp2_sp(ff_vdec_ctx *vdec_ctx, AVFrame *frame, ff_vdec_frame_info *dst_frame)
{
    if ((frame->linesize[YUV42X_DATA_INDEX_U] != frame->linesize[YUV42X_DATA_INDEX_V]) ||
        (frame->height > VIDEO_WIDTH_HEIGHT_MAX) || (frame->linesize[YUV42X_DATA_INDEX_Y] > VIDEO_WIDTH_HEIGHT_MAX) ||
        (frame->linesize[YUV42X_DATA_INDEX_U] > VIDEO_WIDTH_HEIGHT_MAX)) {
        AV_TRACE("yuv42_x U, V linesize is not same,or datat is bigger error\n");
        return TD_FAILURE;
    }

    td_s32 u_v_height = 0;
    ot_pixel_format pixfmt = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    ff_vdecoder_get_info(frame, &u_v_height, &pixfmt);

    ff_vdec_frame_info_part info[AV_NUM_DATA_POINTERS] = { 0 };
    info[YUV42X_DATA_INDEX_Y].line_sizes = frame->linesize[YUV42X_DATA_INDEX_Y];
    info[YUV42X_DATA_INDEX_U].line_sizes  = frame->linesize[YUV42X_DATA_INDEX_U] + frame->linesize[YUV42X_DATA_INDEX_V];

    td_u32 ydata_size = info[YUV42X_DATA_INDEX_Y].line_sizes * frame->height;
    td_u32 y_plane_size = dec_align(ydata_size, ADDR_ALIGIN);
    if ((u_v_height == 0) || (info[YUV42X_DATA_INDEX_U].line_sizes > (INT_MAX / u_v_height)) ||
        ((info[YUV42X_DATA_INDEX_U].line_sizes * u_v_height) / ADDR_ALIGIN + 1) > (INT_MAX / ADDR_ALIGIN)) {
        AV_TRACE(" u_v_height is 0 or v_udata_size too large \n");
        return TD_FAILURE;
    }

    td_u32 v_udata_size = info[YUV42X_DATA_INDEX_U].line_sizes * u_v_height;

    if (ff_vdecoder_create_uv_cache(vdec_ctx, v_udata_size)) {
        return TD_FAILURE;
    }

    dst_frame->frame_data.video_frame.pixel_format = pixfmt;
    td_u32 total_size = y_plane_size + dec_align(v_udata_size, ADDR_ALIGIN);

    vb_vdec_buffer *vb_buf = TD_NULL;
    td_s32 ret = ff_vdecoder_create_vb_vdec(&vb_buf, total_size);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    info[YUV42X_DATA_INDEX_Y].adst_ptr = vb_buf->vir_addr;
    info[YUV42X_DATA_INDEX_U].adst_ptr = (td_u8 *)(vb_buf->vir_addr) + y_plane_size;

    /* copy Y data */
    if (memcpy_s(info[YUV42X_DATA_INDEX_Y].adst_ptr, ydata_size, frame->data[YUV42X_DATA_INDEX_Y], ydata_size) != EOK) {
        goto FAILURE_FRRE_VB_BUF;
    }

    /* interleave V U data to malloc buffer to fast copy speed, vb op is slow without cache */
    ff_vdecoder_interleave_uv(frame, u_v_height, vdec_ctx->uv_cache_buf);
    /* copy interleave VU data */
    if (memcpy_s(info[YUV42X_DATA_INDEX_U].adst_ptr, v_udata_size, vdec_ctx->uv_cache_buf, v_udata_size) != EOK) {
        goto FAILURE_FRRE_VB_BUF;
    }

    ff_vdecoder_set_vdec_frame_info(info, AV_NUM_DATA_POINTERS, vb_buf, dst_frame);

    if (vdec_ctx->codec_ctx->codec_id == AV_CODEC_ID_MJPEG) {
        (td_void)ss_mpi_sys_flush_cache(vb_buf->phy_addr, vb_buf->vir_addr, total_size);
    }
    return TD_SUCCESS;
FAILURE_FRRE_VB_BUF:
    ot_vdec_frame_buffer_free(vb_buf);
    free(vb_buf);
    return TD_FAILURE;
}

static td_void ff_vdecoder_clear_input_streams(ff_vdec_ctx *vdec_ctx)
{
    if (vdec_ctx->stream_source.buf_handle) {
        (td_void)ff_vdec_source_reset(&vdec_ctx->stream_source);
    }
}

static td_void ff_vdecoder_release_frame(const ff_vdec_frame_info *frame_info)
{
    vb_vdec_buffer *vb_buf = frame_info->priv;
    if (vb_buf == NULL) {
        AV_TRACE("invalid vb_vdec_buffer info priv, someone change it\n");
        return;
    }
    ot_vdec_frame_buffer_free(vb_buf);
    free(vb_buf);
}

static td_void ff_vdecoder_clear_output_frames(ff_vdec_ctx *vdec_ctx)
{
    if (vdec_ctx->frame_wait_list != NULL) {
        ff_vdec_frame_info *vdec_frame = (ff_vdec_frame_info *)av_list_get_next(vdec_ctx->frame_wait_list, NULL);
        while (vdec_frame != TD_NULL) {
            av_list_del_node(vdec_ctx->frame_wait_list, vdec_frame);
            ff_vdecoder_release_frame(vdec_frame);
            free(vdec_frame);
            vdec_frame = (ff_vdec_frame_info *)av_list_get_next(vdec_ctx->frame_wait_list, NULL);
        };
    }
}

static td_s32 ff_vdecoder_output_frame(ff_vdec_ctx *vdec_ctx, AVFrame *frame)
{
    ff_vdec_frame_info *frame_info;
    if (frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVJ420P &&
        frame->format != AV_PIX_FMT_YUV422P && frame->format != AV_PIX_FMT_YUVJ422P) {
        AV_TRACE("decode output pixfmt is not supported just support 420p/422p ,cur fmt: %s\n",
            av_get_pix_fmt_name(frame->format));

        return TD_FAILURE;
    }

    td_u32 node_cnt = 0;
    if (av_list_count(vdec_ctx->frame_wait_list, &node_cnt) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    if (node_cnt >= MAX_CACHE_FRAME_NUM) {
        return ERR_VDEC_FRAME_FULL;
    }

    frame_info = (ff_vdec_frame_info *)malloc(sizeof(ff_vdec_frame_info));
    if (frame_info == NULL) {
        AV_TRACE("malloc ff_vdec_frame_info failed\n");
        return TD_FAILURE;
    }

    if (memset_s(frame_info, sizeof(ff_vdec_frame_info), 0x00, sizeof(ff_vdec_frame_info)) != EOK) {
        AV_TRACE("set ff_vdec_frame_info failed\n");
        free(frame_info);
        return TD_FAILURE;
    }

    td_s32 ret = ff_vdecoder_soft_yuv42_xp2_sp(vdec_ctx, frame, frame_info);
    if (ret != TD_SUCCESS) {
        free(frame_info);
        return ret;
    }

    frame_info->frame_data.video_frame.video_format = OT_VIDEO_FORMAT_LINEAR;

    frame_info->frame_data.video_frame.compress_mode = OT_COMPRESS_MODE_NONE;
    frame_info->frame_data.video_frame.height = frame->height;
    frame_info->frame_data.video_frame.width = frame->width;
    frame_info->frame_data.video_frame.pts = (td_u64)frame->pkt_dts;
    frame_info->frame_data.video_frame.field = OT_VIDEO_FIELD_FRAME;

    av_list_add_node(vdec_ctx->frame_wait_list, (td_void *)frame_info);

    // we already set refcounted_frames with 1, must release frame here to avoid mem leak
    av_frame_unref(frame);
    return TD_SUCCESS;
}

static td_s32 ff_vdecoder_codec_open(ff_vdec_ctx *vdec_ctx, enum AVCodecID codec_id)
{
    td_s32 ret = TD_SUCCESS;

    const struct AVCodec *i_pcodec = avcodec_find_decoder(codec_id);
    if (i_pcodec == NULL) {
        AV_TRACE("avcodec_find_decoder error \n");
        ret = TD_FAILURE;
        goto err_ret;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(i_pcodec);
    if (codec_ctx == NULL) {
        AV_TRACE("malloc error \n");
        ret = TD_FAILURE;
        goto err_ret;
    }

    av_dict_set(&vdec_ctx->dec_opts, "refcounted_frames", "1", 0);

    /* open it */
    if (avcodec_open2(codec_ctx, i_pcodec, &vdec_ctx->dec_opts) < 0) {
        AV_TRACE("avcodec_open error \n");
        ret = TD_FAILURE;
        goto open_fail;
    }
    vdec_ctx->codec_ctx = codec_ctx;
    return TD_SUCCESS;

open_fail:
    av_free(codec_ctx);
err_ret:
    return ret;
}

static td_void ff_vdecoder_codec_close(ff_vdec_ctx *vdec_ctx)
{
    if (vdec_ctx->codec_ctx != NULL) {
        av_dict_free(&vdec_ctx->dec_opts);
        avcodec_free_context(&vdec_ctx->codec_ctx);
        vdec_ctx->codec_ctx = NULL;
    }
}

static td_s32 ff_vdec_get_frame(const td_void *vdec_hdl, ot_video_frame_info *video_frame_info)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);
    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx->frame_wait_list);

    ff_vdec_frame_info *vdec_frame = (ff_vdec_frame_info *)av_list_get_next(vdec_ctx->frame_wait_list, NULL);
    if (vdec_frame == NULL) {
        return VDEC_ERR_FRAME_BUF_EMPTY;
    }
    if (vdec_frame->readed) {
        AV_TRACE("last readed frame have not been released\n");
        return TD_FAILURE;
    }
    if (memcpy_s(video_frame_info, sizeof(ot_video_frame_info), &vdec_frame->frame_data,
                 sizeof(vdec_frame->frame_data)) != EOK) {
        AV_TRACE("copy video_frame_info fail \n");
        return TD_FAILURE;
    }
    vdec_frame->readed = TD_TRUE;
    return TD_SUCCESS;
}


static td_s32 ff_vdec_release_frame(const td_void *vdec_hdl, const ot_video_frame_info *video_frame_info)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);
    AVPLAY_RETURN_VAL_IF_NULL(video_frame_info);
    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx->frame_wait_list);

    ff_vdec_frame_info *vdec_frame = (ff_vdec_frame_info *)av_list_get_next(vdec_ctx->frame_wait_list, NULL);
    if (vdec_frame == NULL) {
        AV_TRACE("frame list should not be empty\n");
        return TD_FAILURE;
    }

    if (video_frame_info->video_frame.phys_addr[0] != vdec_frame->frame_data.video_frame.phys_addr[0]) {
        AV_TRACE("input frame info  not same as expect\n");
        return TD_FAILURE;
    }

    av_list_del_node(vdec_ctx->frame_wait_list, vdec_frame);
    ff_vdecoder_release_frame(vdec_frame);
    free(vdec_frame);

    return TD_SUCCESS;
}

static td_void ot_mw_usleep(td_s32 usec)
{
    struct timespec req;
    req.tv_sec = (usec) / SEC_TO_USEC;
    req.tv_nsec = ((usec) % SEC_TO_USEC) * SEC_UNIT;
    nanosleep(&req, NULL);
}

static td_s32 ff_vdecoder_decode_video(ff_vdec_ctx *vdec_ctx, const av_vdec_stream *stream_info, AVPacket *pkt,
    AVCodecParserContext *parser)
{
    AVCodecContext *codec_ctx = vdec_ctx->codec_ctx;

    size_t data_size = stream_info->len;
    td_u8* data = stream_info->addr;
    while (data_size > 0) {
        td_s32 ret = av_parser_parse2(parser, codec_ctx, &pkt->data, &pkt->size, data, data_size, AV_NOPTS_VALUE,
           AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            AV_TRACE("parse fail \n");
            break;
        } else {
            data += ret;
            data_size -= ret;
        }
    
        td_bool output_frame = TD_FALSE;
        if (pkt->size <= 0) {
            continue;
        }
        ret = avcodec_send_packet(codec_ctx, pkt);
        if (ret < 0) {
            AV_TRACE("error sending a packet for decoding\n");
            continue;
        }

        ret = avcodec_receive_frame(codec_ctx, vdec_ctx->stav_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            sample_print("error during decoding\n");
            continue;
        }

        output_frame = TD_TRUE;
        while (output_frame && vdec_ctx->got_pic) {
            ret = ff_vdecoder_output_frame(vdec_ctx, vdec_ctx->stav_frame);
            if ((ret == ERR_VDEC_NO_VBBLK) || (ret == ERR_VDEC_FRAME_FULL)) {
                AVPLAY_UNLOCK(vdec_ctx->dec_lock);
                ot_mw_usleep(MAX_SLEEP_WHEN_NO_BUF);
                continue;
            } else if (ret != TD_SUCCESS) {
                av_frame_unref(vdec_ctx->stav_frame);
                AVPLAY_UNLOCK(vdec_ctx->dec_lock);
                break;
            }
            output_frame = TD_FALSE;
        }
    }

    (td_void)ff_vdec_source_release_data(&vdec_ctx->stream_source);
    return TD_SUCCESS;
}

void *ff_vdecoder_decode_thread(void *args)
{
    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)args;
    AVPacket* pkt = { 0 };
    pkt = av_packet_alloc();
    if (!pkt) {
        AV_TRACE("AVPacket_alloc failed \n");
        return NULL;
    }
    AVCodecParserContext *parser;
    parser = av_parser_init(vdec_ctx->codec_ctx->codec->id);
    if (!parser) {
        AV_TRACE("parser not found\n");
        return NULL;
    }
    td_s32 ret;
    av_vdec_stream stream_info = {0, 0, TD_FALSE, TD_FALSE, TD_FALSE, TD_NULL};
    prctl(PR_SET_NAME, "ot_t_ff_dec", 0, 0, 0);

    vdec_ctx->stav_frame = av_frame_alloc();
    if (vdec_ctx->stav_frame == NULL) {
        AV_TRACE("av_frame_alloc failed  \n");
        return NULL;
    }

    while (vdec_ctx->running) {
        AVPLAY_LOCK(vdec_ctx->dec_lock);

        // there no pic for callback, process decoding
        ret = ff_vdecoder_read_stream(&vdec_ctx->stream_source, &stream_info);
        if (ret != TD_SUCCESS) {
            AVPLAY_UNLOCK(vdec_ctx->dec_lock);
            ot_mw_usleep(MAX_SLEEP_WHEN_NO_DATA);
            continue;
        }

        if (ff_vdecoder_decode_video(vdec_ctx, &stream_info, pkt, parser) < 0) {
            AV_TRACE("ff_vdecoder_decode_video < 0 \n");
            AVPLAY_UNLOCK(vdec_ctx->dec_lock);
            break;
        }

        AVPLAY_UNLOCK(vdec_ctx->dec_lock);
    }

    av_frame_unref(vdec_ctx->stav_frame);
    av_free(vdec_ctx->stav_frame);

    vdec_ctx->stav_frame = NULL;
    vdec_ctx->got_pic = 0;
    return NULL;
}

static td_s32 check_and_init_vdec(td_void **vdec_hdl, ot_payload_type vdec_type,
    const av_vdec_attr *vdec_attr, enum AVCodecID *codec_id)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);
    AVPLAY_RETURN_VAL_IF_NULL(vdec_attr);

    if (vdec_type == OT_PT_JPEG) {
        *codec_id = AV_CODEC_ID_MJPEG;
    } else if (vdec_type == OT_PT_H264) {
        *codec_id = AV_CODEC_ID_H264;
    } else if (vdec_type == OT_PT_H265) {
        *codec_id = AV_CODEC_ID_H265;
    } else if (vdec_type == OT_PT_MJPEG) {
        *codec_id = AV_CODEC_ID_MJPEG;
    } else {
        AV_TRACE("unsupported vdec type: %d\n", vdec_type);
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_s32 ff_vdec_create(td_void **vdec_hdl, ot_payload_type vdec_type, const av_vdec_attr *vdec_attr)
{
    enum AVCodecID codec_id = AV_CODEC_ID_NONE;
    td_s32 ret = check_and_init_vdec(vdec_hdl, vdec_type, vdec_attr, &codec_id);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    
    ff_vdec_ctx *vdec_ctx = malloc(sizeof(ff_vdec_ctx));
    if (vdec_ctx == NULL) {
        AV_TRACE("malloc ff_vdec_ctx failed\n");
        return TD_FAILURE;
    }
    if (memset_s(vdec_ctx, sizeof(ff_vdec_ctx), 0x00, sizeof(ff_vdec_ctx)) != EOK) {
        AV_TRACE("set ff_vdec_ctx failed\n");
        free(vdec_ctx);
        return TD_FAILURE;
    }
    vdec_ctx->vdec_attr = *vdec_attr;

    ret = ff_vdecoder_codec_open(vdec_ctx, codec_id);
    if (ret != TD_SUCCESS) {
        free(vdec_ctx);
        return ret;
    }

    ret = ff_vdec_source_create(&vdec_ctx->stream_source, VIDEO_SOURCE_NAME, vdec_attr->buf_size, MBUF_VIDEO_PAYLOAD);
    if (ret != TD_SUCCESS) {
        AV_TRACE("ot_mw_av_source_create for video fail\n");
        ff_vdecoder_codec_close(vdec_ctx);
        free(vdec_ctx);
        return TD_FAILURE;
    }
    vdec_ctx->vdec_type = vdec_type;

    pthread_mutex_init(&vdec_ctx->dec_lock, NULL);

    av_list_create(&vdec_ctx->frame_wait_list, MAX_CACHE_FRAME_NUM);

    *vdec_hdl = vdec_ctx;
    return TD_SUCCESS;
}

static td_s32 ff_vdec_stop(const td_void *vdec_hdl)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);
    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)vdec_hdl;
    td_s32 ret = TD_SUCCESS;

    if (vdec_ctx->started) {
        vdec_ctx->running = TD_FALSE;
        vdec_ctx->got_pic = 0;
        if (vdec_ctx->thread_id) {
            ret = pthread_join(vdec_ctx->thread_id, NULL);
            if (ret != TD_SUCCESS) {
                AV_TRACE("pthread_join vdec_ctx->thread_id failed\n");
            }
            vdec_ctx->thread_id = 0;
        }
        ff_vdecoder_clear_input_streams(vdec_ctx);
        ff_vdecoder_clear_output_frames(vdec_ctx);
    }
    if (vdec_ctx->uv_cache_buf && vdec_ctx->uv_cache_size > 0) {
        free(vdec_ctx->uv_cache_buf);
        vdec_ctx->uv_cache_buf = NULL;
        vdec_ctx->uv_cache_size = 0;
    }
    vdec_ctx->started = TD_FALSE;
    return ret;
}

static td_s32 ff_vdec_destroy(const td_void *vdec_hdl)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);

    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx->frame_wait_list);

    ff_vdec_stop(vdec_ctx);
    ff_vdecoder_codec_close(vdec_ctx);

    if (ff_vdec_source_destory(&vdec_ctx->stream_source) != TD_SUCCESS) {
        AV_TRACE("fail to ff_vdec_source_destory\n");
    }

    pthread_mutex_destroy(&vdec_ctx->dec_lock);

    av_list_destroy(vdec_ctx->frame_wait_list);

    free(vdec_ctx);

    return TD_SUCCESS;
}

static td_s32 ff_vdec_start(const td_void *vdec_hdl)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);
    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)vdec_hdl;

    if (vdec_ctx->started) {
        AV_TRACE("ff_video_decoder already started\n");
        return TD_SUCCESS;
    }
    vdec_ctx->running = TD_TRUE;
    vdec_ctx->got_pic = 1;

    td_s32 ret;
    ret = pthread_create(&vdec_ctx->thread_id, TD_NULL, ff_vdecoder_decode_thread, vdec_ctx);
    if (ret != 0) {
        AV_TRACE("creat ff_vdecoder_decode_thread failed\n");
        return TD_FAILURE;
    }

    vdec_ctx->started = TD_TRUE;
    return TD_SUCCESS;
}

static td_s32 ff_vdec_send_stream(const td_void *vdec_hdl, const av_vdec_stream *in_stream)
{
    AVPLAY_RETURN_VAL_IF_NULL(vdec_hdl);
    AVPLAY_RETURN_VAL_IF_NULL(in_stream);
    ff_vdec_ctx *vdec_ctx = (ff_vdec_ctx *)vdec_hdl;

    if (!(vdec_ctx->started)) {
        AV_TRACE("decoder have not been started\n");
        return TD_FAILURE;
    }

    if (!in_stream->end_of_stream) {
        if (!in_stream->len) {
            AV_TRACE("input stream len 0 err\n");
            return TD_FAILURE;
        }

        return ff_vdecoder_write_stream(&vdec_ctx->stream_source, in_stream);
    }
    return TD_SUCCESS;
}

static td_s32 soft_h264_vdec_create(td_void **vdec_hdl, const av_vdec_attr *vdec_attr)
{
    return ff_vdec_create(vdec_hdl, OT_PT_H264, vdec_attr);
}

static td_s32 soft_h265_vdec_create(td_void **vdec_hdl, const av_vdec_attr *vdec_attr)
{
    return ff_vdec_create(vdec_hdl, OT_PT_H265, vdec_attr);
}

static td_s32 soft_jpeg_vdec_create(td_void **vdec_hdl, const av_vdec_attr *vdec_attr)
{
    return ff_vdec_create(vdec_hdl, OT_PT_JPEG, vdec_attr);
}

static td_s32 soft_mjpeg_vdec_create(td_void **vdec_hdl, const av_vdec_attr *vdec_attr)
{
    return ff_vdec_create(vdec_hdl, OT_PT_MJPEG, vdec_attr);
}

td_s32 v_decoder_get_h264(av_video_decoder *decoder)
{
    if (decoder == NULL) {
        AV_TRACE("decoder is null! \n");
        return TD_FAILURE;
    }
    if (memset_s(decoder, sizeof(av_video_decoder), 0x00, sizeof(av_video_decoder)) != EOK) {
        AV_TRACE("get H264 decoder fail! becase memset_s error! \n");
        return TD_FAILURE;
    }

    if (strcpy_s(decoder->dec_name, sizeof(decoder->dec_name), "H264") != EOK) {
        AV_TRACE("get H264 decoder fail! becase copy name error! \n");
        return TD_FAILURE;
    }
    decoder->type = OT_PT_H264;
    decoder->pfn_create = soft_h264_vdec_create;
    decoder->pfn_destroy = ff_vdec_destroy;
    decoder->pfn_start = ff_vdec_start;
    decoder->pfn_stop = ff_vdec_stop;
    decoder->pfn_send_stream = ff_vdec_send_stream;
    decoder->pfn_get_frame = ff_vdec_get_frame;
    decoder->pfn_release_frame = ff_vdec_release_frame;
    return TD_SUCCESS;
}

td_s32 v_decoder_get_h265(av_video_decoder *decoder)
{
    if (decoder == NULL) {
        AV_TRACE("decoder is null! \n");
        return TD_FAILURE;
    }
    if (memset_s(decoder, sizeof(av_video_decoder), 0x00, sizeof(av_video_decoder)) != EOK) {
        AV_TRACE("get H265 decoder fail! becase memset_s error! \n");
        return TD_FAILURE;
    }

    if (strcpy_s(decoder->dec_name, sizeof(decoder->dec_name), "H265") != EOK) {
        AV_TRACE("get H264 decoder fail! becase copy name error! \n");
        return TD_FAILURE;
    }
    decoder->type = OT_PT_H265;
    decoder->pfn_create = soft_h265_vdec_create;
    decoder->pfn_destroy = ff_vdec_destroy;
    decoder->pfn_start = ff_vdec_start;
    decoder->pfn_stop = ff_vdec_stop;
    decoder->pfn_send_stream = ff_vdec_send_stream;
    decoder->pfn_get_frame = ff_vdec_get_frame;
    decoder->pfn_release_frame = ff_vdec_release_frame;
    return TD_SUCCESS;
}

td_s32 v_decoder_get_jpeg(av_video_decoder *decoder)
{
    if (decoder == NULL) {
        AV_TRACE("decoder is null! \n");
        return TD_FAILURE;
    }
    if (memset_s(decoder, sizeof(av_video_decoder), 0x00, sizeof(av_video_decoder)) != EOK) {
        AV_TRACE("get JPEG decoder fail! becase memset_s error! \n");
        return TD_FAILURE;
    }

    if (strcpy_s(decoder->dec_name, sizeof(decoder->dec_name), "JPEG") != EOK) {
        AV_TRACE("get JPEG decoder fail! becase copy name error! \n");
        return TD_FAILURE;
    }
    decoder->type = OT_PT_JPEG;
    decoder->pfn_create = soft_jpeg_vdec_create;
    decoder->pfn_destroy = ff_vdec_destroy;
    decoder->pfn_start = ff_vdec_start;
    decoder->pfn_stop = ff_vdec_stop;
    decoder->pfn_send_stream = ff_vdec_send_stream;
    decoder->pfn_get_frame = ff_vdec_get_frame;
    decoder->pfn_release_frame = ff_vdec_release_frame;

    return TD_SUCCESS;
}

td_s32 v_decoder_get_mjpeg(av_video_decoder *decoder)
{
    if (decoder == NULL) {
        AV_TRACE("decoder is null! \n");
        return TD_FAILURE;
    }
    if (memset_s(decoder, sizeof(av_video_decoder), 0x00, sizeof(av_video_decoder)) != EOK) {
        AV_TRACE("get MJPEG decoder fail! becase memset_s error! \n");
        return TD_FAILURE;
    }

    if (strcpy_s(decoder->dec_name, sizeof(decoder->dec_name), "MJPEG") != EOK) {
        AV_TRACE("get MJPEG decoder fail! becase copy name error! \n");
        return TD_FAILURE;
    }
    decoder->type = OT_PT_MJPEG;
    decoder->pfn_create = soft_mjpeg_vdec_create;
    decoder->pfn_destroy = ff_vdec_destroy;
    decoder->pfn_start = ff_vdec_start;
    decoder->pfn_stop = ff_vdec_stop;
    decoder->pfn_send_stream = ff_vdec_send_stream;
    decoder->pfn_get_frame = ff_vdec_get_frame;
    decoder->pfn_release_frame = ff_vdec_release_frame;

    return TD_SUCCESS;
}
