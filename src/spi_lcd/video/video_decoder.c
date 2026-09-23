/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "video_decoder.h"

#include "list.h"
#include "vdec_common.h"
#include "avplay_common.h"
#include "vdec_mng.h"
#include "ff_vdec.h"

typedef struct {
    struct list_head list_ptr;
    av_video_decoder video_decoder;
} av_video_dec_node;

typedef struct {
    td_void *decoder_hdl;
    av_video_decoder video_decoder;
    pthread_mutex_t decoder_lock;
} av_vdec_ctx;

static pthread_mutex_t g_dec_list_lock = PTHREAD_MUTEX_INITIALIZER;
static struct list_head g_vdec_type_list;
static td_bool g_is_vdec_inited = TD_FALSE;

static av_video_decoder g_av_h264_vdec;
static av_video_decoder g_av_h265_vdec;
static av_video_decoder g_av_mjpeg_vdec;
static av_video_decoder g_av_jpeg_vdec;

static td_s32 av_vdec_register_decoder(const av_video_decoder *decoder)
{
    if (decoder == NULL) {
        AV_TRACE("videodecoder is null\n");
        return TD_FAILURE;
    }
    AVPLAY_RETURN_VAL_IF_NULL(decoder->dec_name);
    AVPLAY_RETURN_VAL_IF_NULL(decoder->pfn_create);
    AVPLAY_RETURN_VAL_IF_NULL(decoder->pfn_start);
    AVPLAY_RETURN_VAL_IF_NULL(decoder->pfn_stop);
    AVPLAY_RETURN_VAL_IF_NULL(decoder->pfn_send_stream);
    AVPLAY_RETURN_VAL_IF_NULL(decoder->pfn_get_frame);
    AVPLAY_RETURN_VAL_IF_NULL(decoder->pfn_release_frame);

    struct list_head *pos = NULL;
    av_video_dec_node *node = NULL;
    AVPLAY_LOCK(g_dec_list_lock);
    list_for_each(pos, &g_vdec_type_list)
    {
        node = list_entry(pos, av_video_dec_node, list_ptr);
        if (node->video_decoder.type == decoder->type ||
            !strcasecmp(decoder->dec_name, node->video_decoder.dec_name)) {
            AV_TRACE("decoder %s already registered\n", decoder->dec_name);
            AVPLAY_UNLOCK(g_dec_list_lock);
            return TD_FAILURE;
        }
    }
    node = (av_video_dec_node *)malloc(sizeof(av_video_dec_node));
    if (node == NULL) {
        AV_TRACE("malloc av_video_dec_node failed\n");
        AVPLAY_UNLOCK(g_dec_list_lock);
        return TD_FAILURE;
    }
    if (memset_s(node, sizeof(av_video_dec_node), 0x00, sizeof(av_video_dec_node)) != EOK) {
        AV_TRACE("set  av_video_dec_node failed\n");
        free(node);
        node = NULL;
        AVPLAY_UNLOCK(g_dec_list_lock);
        return TD_FAILURE;
    }
    node->video_decoder = *decoder;
    list_add(&node->list_ptr, &g_vdec_type_list);
    AVPLAY_UNLOCK(g_dec_list_lock);
    return TD_SUCCESS;
}

td_s32 av_vdec_un_register_decoder(const av_video_decoder *decoder)
{
    if (decoder == NULL) {
        AV_TRACE("videodecoder is null\n");
        return TD_FAILURE;
    }
    struct list_head *pos = NULL;
    av_video_dec_node *node = NULL;
    td_bool is_exist = TD_FALSE;

    AVPLAY_LOCK(g_dec_list_lock);

    list_for_each(pos, &g_vdec_type_list)
    {
        node = list_entry(pos, av_video_dec_node, list_ptr);
        if (node->video_decoder.type == decoder->type &&
            !strcasecmp(node->video_decoder.dec_name, decoder->dec_name)) {
            is_exist = TD_TRUE;
            break;
        }
    }
    if ((!is_exist) || (node == NULL)) {
        AV_TRACE("could not find registered vdec %d\n", decoder->type);
        AVPLAY_UNLOCK(g_dec_list_lock);
        return TD_FAILURE;
    }
    list_del(&node->list_ptr);
    free(node);
    AVPLAY_UNLOCK(g_dec_list_lock);
    return TD_SUCCESS;
}

td_s32 av_vdec_init(td_void)
{
    if (g_is_vdec_inited) {
        AV_TRACE("VDEC already inited\n");
        return TD_SUCCESS;
    }
    vdec_mng_init();
    INIT_LIST_HEAD(&g_vdec_type_list);
    td_s32 ret = v_decoder_get_h264(&g_av_h264_vdec);
    if (ret != TD_SUCCESS) {
        return ret;
    }
    ret = av_vdec_register_decoder(&g_av_h264_vdec);
    if (ret != TD_SUCCESS) {
        return ret;
    }
    if (v_decoder_get_h265(&g_av_h265_vdec) != TD_SUCCESS) {
        goto UNREG_H264;
    }
    if (av_vdec_register_decoder(&g_av_h265_vdec) != TD_SUCCESS) {
        goto UNREG_H264;
    }
    if (v_decoder_get_mjpeg(&g_av_mjpeg_vdec) != TD_SUCCESS) {
        goto UNREG_H265;
    }
    if (av_vdec_register_decoder(&g_av_mjpeg_vdec) != TD_SUCCESS) {
        goto UNREG_H265;
    }
    if (v_decoder_get_jpeg(&g_av_jpeg_vdec) != TD_SUCCESS) {
        goto UNREG_ALL;
    }
    if (av_vdec_register_decoder(&g_av_jpeg_vdec) != TD_SUCCESS) {
        goto UNREG_ALL;
    }
    g_is_vdec_inited = TD_TRUE;
    return TD_SUCCESS;
UNREG_ALL:
    (void)av_vdec_un_register_decoder(&g_av_mjpeg_vdec);
UNREG_H265:
    (void)av_vdec_un_register_decoder(&g_av_h265_vdec);
UNREG_H264:
    (void)av_vdec_un_register_decoder(&g_av_h264_vdec);
    return TD_FAILURE;
}

td_s32 av_vdec_deinit(td_void)
{
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    av_video_dec_node *node = NULL;

    if (g_is_vdec_inited) {
        td_s32 ret = av_vdec_un_register_decoder(&g_av_h264_vdec);
        if (ret != TD_SUCCESS) {
            AV_TRACE("av_vdec_un_register_decoder failed\n");
        }
        ret = av_vdec_un_register_decoder(&g_av_h265_vdec);
        if (ret != TD_SUCCESS) {
            AV_TRACE("av_vdec_un_register_decoder failed\n");
        }
        ret = av_vdec_un_register_decoder(&g_av_mjpeg_vdec);
        if (ret != TD_SUCCESS) {
            AV_TRACE("av_vdec_un_register_decoder failed\n");
        }
        ret = av_vdec_un_register_decoder(&g_av_jpeg_vdec);
        if (ret != TD_SUCCESS) {
            AV_TRACE("av_vdec_un_register_decoder failed\n");
        }
        AVPLAY_LOCK(g_dec_list_lock);
        list_for_each_safe(pos, n, &g_vdec_type_list)
        {
            node = list_entry(pos, av_video_dec_node, list_ptr);
            list_del(&node->list_ptr);
            free(node);
            node = TD_NULL;
        }
        AVPLAY_UNLOCK(g_dec_list_lock);
        vdec_mng_deinit();
        g_is_vdec_inited = TD_FALSE;
    }
    return TD_SUCCESS;
}

td_s32 av_vdec_create(td_void **vdec_hdl, ot_payload_type vdec_type, const av_vdec_attr *vdec_attr)
{
    av_video_dec_node *node = NULL;
    struct list_head *pos = NULL;
    td_bool is_find = TD_FALSE;

    AVPLAY_LOCK(g_dec_list_lock);
    list_for_each(pos, &g_vdec_type_list) {
        node = list_entry(pos, av_video_dec_node, list_ptr);
        if (node->video_decoder.type == vdec_type) {
            is_find = TD_TRUE;
            break;
        }
    }
    AVPLAY_UNLOCK(g_dec_list_lock);
    if ((!is_find) || (node == NULL)) {
        AV_TRACE("could not find correspond vdec for type: %d\n", vdec_type);
        return TD_FAILURE;
    }
    av_vdec_ctx *dec_ctx = (av_vdec_ctx *)malloc(sizeof(av_vdec_ctx));
    if (dec_ctx == NULL) {
        AV_TRACE("malloc av_vdec_ctx failed\n");
        return TD_FAILURE;
    }
    if (memset_s(dec_ctx, sizeof(av_vdec_ctx), 0x00, sizeof(av_vdec_ctx)) != EOK) {
        AV_TRACE("set av_vdec_ctx failed\n");
        free(dec_ctx);
        return TD_FAILURE;
    }
    dec_ctx->video_decoder = node->video_decoder;
    (td_void)pthread_mutex_init(&dec_ctx->decoder_lock, NULL);

    td_s32 ret = dec_ctx->video_decoder.pfn_create(&dec_ctx->decoder_hdl, vdec_attr);
    if (ret != TD_SUCCESS) {
        AV_TRACE("video_decoder.pfn_create failed ret: %x\n", ret);
        pthread_mutex_destroy(&dec_ctx->decoder_lock);
        free(dec_ctx);
        return ret;
    }
    *vdec_hdl = (td_void *)dec_ctx;
    return TD_SUCCESS;
}

td_s32 av_vdec_destroy(const td_void *vdec_hdl)
{
    av_vdec_ctx *vdec_ctx = (av_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx);
    if (vdec_ctx->decoder_hdl == NULL) {
        AV_TRACE("vdec_ctx->decoder_hdl is null\n");
        return TD_FAILURE;
    }
    td_s32 ret = vdec_ctx->video_decoder.pfn_destroy(vdec_ctx->decoder_hdl);
    if (ret != TD_SUCCESS) {
        AV_TRACE("video_decoder.pfn_destroy failed ret: %x\n", ret);
    }
    (td_void)pthread_mutex_destroy(&vdec_ctx->decoder_lock);
    free(vdec_ctx);
    return ret;
}

td_s32 av_vdec_start(const td_void *vdec_hdl)
{
    av_vdec_ctx *vdec_ctx = (av_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx);

    AVPLAY_LOCK(vdec_ctx->decoder_lock);
    if (vdec_ctx->decoder_hdl == NULL) {
        AV_TRACE("vdec_ctx->decoder_hdl is null\n");
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return TD_FAILURE;
    }
    td_s32 ret = vdec_ctx->video_decoder.pfn_start(vdec_ctx->decoder_hdl);
    if (ret != TD_SUCCESS) {
        AV_TRACE("video_decoder.pfn_start failed ret: %x\n", ret);
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return ret;
    }
    AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
    return TD_SUCCESS;
}

td_s32 av_vdec_stop(const td_void *vdec_hdl)
{
    av_vdec_ctx *vdec_ctx = (av_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx);

    AVPLAY_LOCK(vdec_ctx->decoder_lock);
    if (vdec_ctx->decoder_hdl == NULL) {
        AV_TRACE("vdec_ctx->decoder_hdl is null\n");
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return TD_FAILURE;
    }
    td_s32 ret = vdec_ctx->video_decoder.pfn_stop(vdec_ctx->decoder_hdl);
    if (ret != TD_SUCCESS) {
        AV_TRACE("video_decoder.pfn_stop failed ret: %x\n", ret);
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return ret;
    }
    AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
    return TD_SUCCESS;
}

td_s32 av_vdec_send_stream(const td_void *vdec_hdl, const av_vdec_stream *vdec_stream)
{
    av_vdec_ctx *vdec_ctx = (av_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx);
    AVPLAY_RETURN_VAL_IF_NULL(vdec_stream);

    AVPLAY_LOCK(vdec_ctx->decoder_lock);
    if (vdec_ctx->decoder_hdl == NULL) {
        AV_TRACE("vdec_ctx->decoder_hdl is null\n");
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return TD_FAILURE;
    }
    td_s32 ret = vdec_ctx->video_decoder.pfn_send_stream(vdec_ctx->decoder_hdl, vdec_stream);
    if (ret != TD_SUCCESS) {
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return ret;
    }
    AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
    return TD_SUCCESS;
}

td_s32 av_vdec_get_frame(const td_void *vdec_hdl, ot_video_frame_info *video_frame_info)
{
    av_vdec_ctx *vdec_ctx = (av_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx);
    AVPLAY_RETURN_VAL_IF_NULL(video_frame_info);

    AVPLAY_LOCK(vdec_ctx->decoder_lock);
    if (vdec_ctx->decoder_hdl == NULL) {
        AV_TRACE("vdec_ctx->decoder_hdl is null\n");
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return TD_FAILURE;
    }
    td_s32 ret = vdec_ctx->video_decoder.pfn_get_frame(vdec_ctx->decoder_hdl, video_frame_info);
    if (ret != TD_SUCCESS) {
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return ret;
    }
    AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
    return TD_SUCCESS;
}

td_s32 av_vdec_release_frame(const td_void *vdec_hdl, const ot_video_frame_info *video_frame_info)
{
    av_vdec_ctx *vdec_ctx = (av_vdec_ctx *)vdec_hdl;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_ctx);
    AVPLAY_RETURN_VAL_IF_NULL(video_frame_info);

    AVPLAY_LOCK(vdec_ctx->decoder_lock);
    if (vdec_ctx->decoder_hdl == NULL) {
        AV_TRACE("vdec_ctx->decoder_hdl is null\n");
        AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
        return TD_FAILURE;
    }
    td_s32 ret = vdec_ctx->video_decoder.pfn_release_frame(vdec_ctx->decoder_hdl, video_frame_info);
    if (ret != TD_SUCCESS) {
        AV_TRACE("video_decoder.pfn_release_frame failed ret: %x\n", ret);
    }

    AVPLAY_UNLOCK(vdec_ctx->decoder_lock);
    return ret;
}
