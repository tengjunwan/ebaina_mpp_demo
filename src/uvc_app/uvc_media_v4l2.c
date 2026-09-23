/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <unistd.h>
#include <sys/prctl.h>
#include "log.h"
#include "sample_venc.h"
#include "camera.h"
#include "ss_mpi_ae.h"
#include "ss_mpi_awb.h"
#include "uvc.h"
#include "frame_cache.h"
#include "uvc_media.h"


/* UVC dev_no should always equal to UVC chn, equal to X in /dev/videoX */
static uvc_media_cfg g_media_cfg[OT_UVC_MAX_CHN_NUM] = {0};
static td_bool g_is_media_start[OT_UVC_MAX_CHN_NUM] = {TD_FALSE};
static encoder_property g_encoder_property[OT_UVC_MAX_CHN_NUM] = {0};
static pthread_t g_thread_send_data[OT_UVC_MAX_CHN_NUM];


static td_void sample_uvc_get_default_pic_buf_attr(ot_size *pic_size, ot_pic_buf_attr *buf_attr)
{
    buf_attr->width         = pic_size->width;
    buf_attr->height        = pic_size->height;
    buf_attr->bit_width     = OT_DATA_BIT_WIDTH_8;
    buf_attr->pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    buf_attr->compress_mode = OT_COMPRESS_MODE_NONE;
    buf_attr->align         = OT_DEFAULT_ALIGN;
    buf_attr->video_format  = OT_VIDEO_FORMAT_LINEAR;
}

static td_void sample_uvc_get_sensor_size(sample_sns_type sns_type, ot_size *sensor_size)
{
    ot_isp_pub_attr pub_attr;
    (td_void)sample_comm_isp_get_pub_attr_by_sns(sns_type, &pub_attr); /* it return success or default */
    sensor_size->width = pub_attr.sns_size.width;
    sensor_size->height = pub_attr.sns_size.height;
}

static td_bool sample_uvc_is_need_venc(unsigned int format)
{
    if ((format != VIDEO_IMG_FORMAT_YUYV) && (format != VIDEO_IMG_FORMAT_YUV420) &&
        (format != VIDEO_IMG_FORMAT_NV21) && (format != VIDEO_IMG_FORMAT_NV12)) {
        return TD_TRUE;
    } else {
        return TD_FALSE;
    }
}

static void sample_uvc_config_vb(sample_sns_type sns_type, ot_vb_cfg *vb_cfg)
{
    ot_size              sensor_size;
    ot_pic_buf_attr      buf_attr;

    sample_uvc_get_sensor_size(sns_type, &sensor_size);
    sample_uvc_get_default_pic_buf_attr(&sensor_size, &buf_attr);

    if (camera_get_uvc_device_cnt() == 1) { /* VI-online-VPSS-online */
        vb_cfg->max_pool_cnt = 2; /* 2 pools */

        buf_attr.pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        buf_attr.width = 2560; /* 2560 max width */
        buf_attr.height = 1440; /* 1440 max height */
        vb_cfg->common_pool[0].blk_size = ot_common_get_pic_buf_size(&buf_attr);
        vb_cfg->common_pool[0].blk_cnt = 4; /* 4: blk cnt */
        vb_cfg->common_pool[0].remap_mode = OT_VB_REMAP_MODE_CACHED;

        buf_attr.pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
        buf_attr.width = 1920; /* 1920 width */
        buf_attr.height = 1080; /* 1080 height */
        vb_cfg->common_pool[1].blk_size = ot_common_get_pic_buf_size(&buf_attr);
        vb_cfg->common_pool[1].blk_cnt = 2; /* 2: blk cnt */
        vb_cfg->common_pool[1].remap_mode = OT_VB_REMAP_MODE_CACHED;
    } else { /* 2 device 2 pipe, VI-offline-VPSS-online */
        vb_cfg->max_pool_cnt = 3; /* 3 pool */

        buf_attr.compress_mode = OT_COMPRESS_MODE_LINE;
        buf_attr.pixel_format  = OT_PIXEL_FORMAT_RGB_BAYER_10BPP;
        vb_cfg->common_pool[0].blk_size = ot_common_get_pic_buf_size(&buf_attr);
        vb_cfg->common_pool[0].blk_cnt = 7;  /* 7: blk cnt */
        vb_cfg->common_pool[0].remap_mode = OT_VB_REMAP_MODE_CACHED;

        buf_attr.compress_mode = OT_COMPRESS_MODE_NONE;
        buf_attr.pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        buf_attr.width = 1920; /* 1920 width */
        buf_attr.height = 1080; /* 1080 height */
        vb_cfg->common_pool[1].blk_size = ot_common_get_pic_buf_size(&buf_attr);
        vb_cfg->common_pool[1].blk_cnt = 2; /* 2: blk cnt */
        vb_cfg->common_pool[1].remap_mode = OT_VB_REMAP_MODE_CACHED;

        buf_attr.pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
        buf_attr.width = 640; /* 640 width */
        buf_attr.height = 360; /* 360 height */
        vb_cfg->common_pool[0x2].blk_size = ot_common_get_pic_buf_size(&buf_attr);
        vb_cfg->common_pool[0x2].blk_cnt = 2; /* 2: blk cnt */
        vb_cfg->common_pool[0x2].remap_mode = OT_VB_REMAP_MODE_CACHED;
    }
}

static td_s32 sample_uvc_sys_init(sample_sns_type sns_type, td_u32 supplement_cfg)
{
    ot_vb_cfg            vb_cfg = {0};
    ot_vb_supplement_cfg vb_supplement_cfg;
    td_s32               ret;

    sample_uvc_config_vb(sns_type, &vb_cfg);
    vb_supplement_cfg.supplement_cfg = supplement_cfg;
    ret = ss_mpi_vb_set_supplement_cfg(&vb_supplement_cfg);
    if (ret != TD_SUCCESS) {
        sample_print("set vb cfg failed!\n");
        return TD_FAILURE;
    }

    ret = sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_cfg);
    if (ret != TD_SUCCESS) {
        sample_print("sys_init_with_vb failed %#x\n", ret);
        return TD_FAILURE;
    }

    if (camera_get_uvc_device_cnt() == 1) {
        sample_comm_vi_set_vi_vpss_mode(OT_VI_ONLINE_VPSS_ONLINE, OT_VI_AIISP_MODE_DEFAULT);
    } else {
        sample_comm_vi_set_vi_vpss_mode(OT_VI_OFFLINE_VPSS_ONLINE, OT_VI_AIISP_MODE_DEFAULT);
    }

    return TD_SUCCESS;
}

static td_s32 sample_uvc_vi_init(uvc_media_cfg *media_cfg)
{
    td_s32 ret;
    td_u32 dev_no = media_cfg->dev_no;
    ot_vi_pipe vi_pipe = media_cfg->vi_pipe;
    ot_vi_dev vi_dev;

    sample_comm_vi_get_default_vi_cfg(media_cfg->sns_type, &media_cfg->vi_cfg);
    if (dev_no == 1) {
        vi_dev = 1;
        media_cfg->vi_cfg.sns_info.bus_id = 1;
        media_cfg->vi_cfg.sns_info.sns_clk_src = 1;
        media_cfg->vi_cfg.sns_info.sns_rst_src = 1;
        sample_comm_vi_get_mipi_info_by_dev_id(media_cfg->sns_type, vi_dev, &media_cfg->vi_cfg.mipi_info);
        media_cfg->vi_cfg.dev_info.vi_dev = vi_dev;
        media_cfg->vi_cfg.bind_pipe.pipe_id[0] = vi_pipe;
        media_cfg->vi_cfg.grp_info.grp_num = 1;
        media_cfg->vi_cfg.grp_info.fusion_grp[0] = 1;
        media_cfg->vi_cfg.grp_info.fusion_grp_attr[0].pipe_id[0] = vi_pipe;
        media_cfg->vi_cfg.mipi_info.divide_mode = LANE_DIVIDE_MODE_1;
    }

    for (td_s32 i = 0; i < OT_VI_MAX_PHYS_PIPE_NUM; i++) {
        if (media_cfg->vi_cfg.pipe_info[i].pipe_need_start == TD_TRUE) {
            media_cfg->vi_cfg.pipe_info[i].nr_attr.nr_type = OT_NR_TYPE_VIDEO_SPATIAL;
            media_cfg->vi_cfg.pipe_info[i].chn_info[0].chn_attr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
        }
    }
    ret = sample_comm_vi_start_vi(&media_cfg->vi_cfg);

    return ret;
}

static td_void sample_uvc_get_default_vpss_ext_chn_attr(ot_vpss_ext_chn_attr *vpss_ext_chn_attr)
{
    vpss_ext_chn_attr->bind_chn = 0; /* VPSS phys chn */
    vpss_ext_chn_attr->src_type = OT_EXT_CHN_SRC_TYPE_TAIL;
    vpss_ext_chn_attr->width = 1920;     /* 1920: default width */
    vpss_ext_chn_attr->height = 1080;    /* 1080: default height */
    vpss_ext_chn_attr->depth = 1;
    vpss_ext_chn_attr->video_format = OT_VIDEO_FORMAT_LINEAR;
    vpss_ext_chn_attr->dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    vpss_ext_chn_attr->pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    vpss_ext_chn_attr->compress_mode = OT_COMPRESS_MODE_NONE;
    vpss_ext_chn_attr->frame_rate.src_frame_rate = -1;
    vpss_ext_chn_attr->frame_rate.dst_frame_rate = -1;
}


static td_s32 sample_uvc_vpss_init(uvc_media_cfg *media_cfg)
{
    td_s32 ret;
    ot_low_delay_info low_delay_info;
    ot_vpss_chn vpss_ext_chn = media_cfg->vpss_ext_chn;
    ot_vpss_ext_chn_attr vpss_ext_chn_attr;
    td_bool is_need_venc = sample_uvc_is_need_venc(g_encoder_property[media_cfg->dev_no].format);
    sample_vpss_chn_attr vpss_chn_attr = {0};

    memcpy_s(&vpss_chn_attr.chn_attr[0], sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
        media_cfg->vpss_chn_attr, sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
    memcpy_s(vpss_chn_attr.chn_enable, sizeof(td_bool) * OT_VPSS_MAX_PHYS_CHN_NUM,
        media_cfg->vpss_chn_enable, sizeof(td_bool) * OT_VPSS_MAX_PHYS_CHN_NUM);
    vpss_chn_attr.chn_array_size = OT_VPSS_MAX_PHYS_CHN_NUM;
    ret = sample_common_vpss_start(media_cfg->vpss_grp, &media_cfg->vpss_grp_attr, &vpss_chn_attr);
    if (ret != TD_SUCCESS) {
        sample_print("start vpss failed!\n");
        return TD_FAILURE;
    }

    if (g_encoder_property[media_cfg->dev_no].format == VIDEO_IMG_FORMAT_YUYV) {
        sample_uvc_get_default_vpss_ext_chn_attr(&vpss_ext_chn_attr);
        vpss_ext_chn_attr.width = media_cfg->vpss_chn_attr[media_cfg->vpss_chn].width;
        vpss_ext_chn_attr.height = media_cfg->vpss_chn_attr[media_cfg->vpss_chn].height;
        vpss_ext_chn_attr.pixel_format = OT_PIXEL_FORMAT_VY1UY0_PACKAGE_422;
        ret = ss_mpi_vpss_set_ext_chn_attr(media_cfg->vpss_grp, vpss_ext_chn, &vpss_ext_chn_attr);
        if (ret != TD_SUCCESS) {
            sample_print("set vpss ext_chn%d attr failed!\n", vpss_ext_chn);
            goto vpss_fail;
        }

        ret = ss_mpi_vpss_enable_chn(media_cfg->vpss_grp, vpss_ext_chn);
        if (ret != TD_SUCCESS) {
            sample_print("enable vpss ext chn%d failed!\n", vpss_ext_chn);
            goto vpss_fail;
        }
    }

    if (is_need_venc == TD_TRUE) {
        low_delay_info.enable = TD_TRUE;
        low_delay_info.line_cnt = 16;       /* 16: low delay line */
        low_delay_info.one_buf_en = TD_FALSE;
        ret = ss_mpi_vpss_set_chn_low_delay(media_cfg->vpss_grp, media_cfg->vpss_chn, &low_delay_info);
        if (ret != TD_SUCCESS) {
            sample_print("set vpss chn low delay failed!\n");
            goto vpss_fail;
        }
    }

    return TD_SUCCESS;

vpss_fail:
   sample_common_vpss_stop(media_cfg->vpss_grp, media_cfg->vpss_chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
   return TD_FAILURE;
}

static td_void sample_uvc_vpss_de_init(uvc_media_cfg *media_cfg)
{
    if (g_encoder_property[media_cfg->dev_no].format == VIDEO_IMG_FORMAT_YUYV) {
        (td_void)ss_mpi_vpss_disable_chn(media_cfg->vpss_grp, media_cfg->vpss_ext_chn);
    }
    (td_void)sample_common_vpss_stop(media_cfg->vpss_grp, media_cfg->vpss_chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
}

static td_void sample_venc_set_one_stream_buf(td_void)
{
    td_s32 ret;
    ot_venc_mod_param mod_param = {0};
    mod_param.mod_type = OT_VENC_MOD_H265;
    ret = ss_mpi_venc_get_mod_param(&mod_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc_get_mod_param 265 ret %#x\n", ret);
    }
    mod_param.h265_mod_param.one_stream_buf = 1;
    ret = ss_mpi_venc_set_mod_param(&mod_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc_set_mod_param 265 ret %#x\n", ret);
    }

    (td_void)memset_s(&mod_param, sizeof(ot_venc_mod_param), 0, sizeof(ot_venc_mod_param));
    mod_param.mod_type = OT_VENC_MOD_H264;
    ret = ss_mpi_venc_get_mod_param(&mod_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc_get_mod_param 264 ret %#x\n", ret);
    }
    mod_param.h264_mod_param.one_stream_buf = 1;
    ret = ss_mpi_venc_set_mod_param(&mod_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc_set_mod_param 264 ret %#x\n", ret);
    }

    (td_void)memset_s(&mod_param, sizeof(ot_venc_mod_param), 0, sizeof(ot_venc_mod_param));
    mod_param.mod_type = OT_VENC_MOD_JPEG;
    ret = ss_mpi_venc_get_mod_param(&mod_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc_get_mod_param jpeg ret %#x\n", ret);
    }
    mod_param.jpeg_mod_param.one_stream_buf = 1;
    ret = ss_mpi_venc_set_mod_param(&mod_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc_set_mod_param jpeg ret %#x\n", ret);
    }
}

static td_s32 sample_uvc_venc_init(uvc_media_cfg *media_cfg)
{
    td_s32 ret;
    ot_venc_chn venc_chn = media_cfg->venc_chn;

    if (media_cfg->venc_chn_param.type == OT_PT_H264) {
        media_cfg->venc_chn_param.rc_mode = SAMPLE_RC_AVBR;
    }

    ret = sample_comm_venc_start(venc_chn, &media_cfg->venc_chn_param);
    if (ret != TD_SUCCESS) {
        sample_print("start venc failed!\n");
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_void sample_uvc_get_default_venc_chn_param(sample_comm_venc_chn_param *venc_chn_param)
{
    venc_chn_param->frame_rate = 30;            /* 30: default frame rate */
    venc_chn_param->stats_time = 1;
    venc_chn_param->gop = 30;                   /* 30: default gop */
    venc_chn_param->venc_size.width = 1920;     /* 1920: default width */
    venc_chn_param->venc_size.height = 1080;    /* 1080: default height */
    venc_chn_param->profile = 0;
    venc_chn_param->is_rcn_ref_share_buf = TD_FALSE;
    venc_chn_param->gop_attr.gop_mode = OT_VENC_GOP_MODE_NORMAL_P;
    venc_chn_param->gop_attr.normal_p.ip_qp_delta = 2;  /* 2: default ip_qp_delta */
    venc_chn_param->type = OT_PT_H264;
    venc_chn_param->rc_mode = SAMPLE_RC_CBR;
}

static td_void sample_uvc_media_cfg(td_u32 dev_no, uvc_media_cfg *media_cfg)
{
    td_u32 i;
    ot_size sensor_size;

    media_cfg->sns_type = sample_get_sns_type(dev_no);

    sample_uvc_get_sensor_size(media_cfg->sns_type, &sensor_size);
    media_cfg->vi_pipe = (ot_vi_pipe)dev_no;
    media_cfg->vi_chn = 0;

    sample_comm_vpss_get_default_grp_attr(&media_cfg->vpss_grp_attr);
    media_cfg->vpss_grp_attr.max_width = sensor_size.width;
    media_cfg->vpss_grp_attr.max_height = sensor_size.height;
    media_cfg->vpss_grp_attr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;

    for (i = 0; i < OT_VPSS_MAX_PHYS_CHN_NUM; i++) {
        sample_comm_vpss_get_default_chn_attr(&media_cfg->vpss_chn_attr[i]);
        media_cfg->vpss_chn_attr[i].width = sensor_size.width;
        media_cfg->vpss_chn_attr[i].height = sensor_size.height;
        media_cfg->vpss_chn_enable[i] = TD_FALSE;
        media_cfg->vpss_chn_attr[i].compress_mode = OT_COMPRESS_MODE_NONE;
    }
    media_cfg->vpss_grp = (ot_vpss_grp)dev_no;
    media_cfg->vpss_chn = 0;
    media_cfg->vpss_ext_chn = OT_VPSS_MAX_PHYS_CHN_NUM;
    media_cfg->vpss_chn_enable[media_cfg->vpss_chn] = TD_TRUE;

    media_cfg->venc_chn = (ot_venc_chn)dev_no;
    sample_uvc_get_default_venc_chn_param(&media_cfg->venc_chn_param);

    media_cfg->supplement_cfg = OT_VB_SUPPLEMENT_BNR_MOT_MASK;
    media_cfg->dev_no = dev_no;
}

static td_s32 sample_uvc_init_and_bind_vi_vpss(uvc_media_cfg *media_cfg)
{
    td_s32 ret;

    ret = sample_uvc_vi_init(media_cfg);
    if (ret != TD_SUCCESS) {
        sample_print("vi init failed!\n");
        return TD_FAILURE;
    }

    ret = sample_uvc_vpss_init(media_cfg);
    if (ret != TD_SUCCESS) {
        sample_print("vpss init failed!\n");
        goto vi_exit;
    }

    ret = sample_comm_vi_bind_vpss(media_cfg->vi_pipe, media_cfg->vi_chn, media_cfg->vpss_grp, media_cfg->vpss_chn);
    if (ret != TD_SUCCESS) {
        sample_print("vi(%d, %d) bind vpss(%d, %d) failed!\n", media_cfg->vi_pipe, media_cfg->vi_chn,
            media_cfg->vpss_grp, media_cfg->vpss_chn);
        goto vpss_exit;
    }
    return TD_SUCCESS;

vpss_exit:
    sample_uvc_vpss_de_init(media_cfg);
vi_exit:
    sample_comm_vi_stop_vi(&media_cfg->vi_cfg);
    return TD_FAILURE;
}

static td_s32 sampe_uvc_start_vi_vpss_venc(uvc_media_cfg *media_cfg)
{
    td_s32 ret = sample_uvc_init_and_bind_vi_vpss(media_cfg);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    td_u32 fcc_format = g_encoder_property[media_cfg->dev_no].format;
    if (sample_uvc_is_need_venc(fcc_format) == TD_TRUE) {
        ret = sample_uvc_venc_init(media_cfg);
        if (ret != TD_SUCCESS) {
            sample_print("venc init failed!\n");
            goto vi_unbind_vpss;
        }

        ret = sample_comm_vpss_bind_venc(media_cfg->vpss_grp, media_cfg->vpss_chn, media_cfg->venc_chn);
        if (ret != TD_SUCCESS) {
            goto venc_exit;
        }
    }

    return TD_SUCCESS;

venc_exit:
    sample_comm_venc_stop(media_cfg->venc_chn);
vi_unbind_vpss:
    sample_comm_vi_un_bind_vpss(media_cfg->vi_pipe, media_cfg->vi_chn, media_cfg->vpss_grp, media_cfg->vpss_chn);
    sample_uvc_vpss_de_init(media_cfg);
    sample_comm_vi_stop_vi(&media_cfg->vi_cfg);

    return TD_FAILURE;
}

static td_void sample_uvc_stop_vi_vpss_venc(uvc_media_cfg *media_cfg)
{
    ot_vi_pipe    vi_pipe      = media_cfg->vi_pipe;
    ot_vi_chn     vi_chn       = media_cfg->vi_chn;
    sample_vi_cfg *vi_cfg      = &media_cfg->vi_cfg;
    ot_vpss_grp   vpss_grp     = media_cfg->vpss_grp;
    ot_vpss_chn   vpss_chn     = media_cfg->vpss_chn;
    ot_venc_chn   venc_chn     = media_cfg->venc_chn;
    td_bool       is_need_venc = sample_uvc_is_need_venc(g_encoder_property[media_cfg->dev_no].format);
    if (is_need_venc == TD_TRUE) {
        (td_void)ss_mpi_venc_stop_chn(venc_chn); // need stop venc chn firstly when venc unbind uvc.
        sample_comm_vpss_un_bind_venc(vpss_grp, vpss_chn, venc_chn);
        sample_comm_venc_stop(venc_chn);
    }

    sample_comm_vi_un_bind_vpss(vi_pipe, vi_chn, vpss_grp, vpss_chn);
    sample_uvc_vpss_de_init(media_cfg);
    sample_comm_vi_stop_vi(vi_cfg);
}

static td_s32 sample_uvc_media_route(td_u32 dev_no)
{
    td_s32 ret;
    ot_pic_size pic_size = PIC_1080P;
    ot_size stream_size;
    ot_vpss_chn vpss_chn;

    sample_uvc_media_cfg(dev_no, &g_media_cfg[dev_no]);

    /* edit media cfg here */
    set_user_config_format(dev_no, &g_media_cfg[dev_no].venc_chn_param.type, &pic_size,
        &g_media_cfg[dev_no].venc_chn_num);
    ret = sample_comm_sys_get_pic_size(pic_size, &stream_size);
    if (ret != TD_SUCCESS) {
        sample_print("get stream_size failed!\n");
        return ret;
    }

    g_media_cfg[dev_no].venc_chn_param.venc_size.width = stream_size.width;
    g_media_cfg[dev_no].venc_chn_param.venc_size.height = stream_size.height;
    g_media_cfg[dev_no].venc_chn_param.size = pic_size;

    vpss_chn = g_media_cfg[dev_no].vpss_chn;
    g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].width = stream_size.width;
    g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].height = stream_size.height;
    g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].frame_rate.src_frame_rate = (td_s32)FRAME_INTERVAL_MAX;
    g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].frame_rate.dst_frame_rate = (td_s32)g_encoder_property[dev_no].fps;
    if (g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_NV12 ||
        g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_NV21) {
        g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].depth = 1; /* for user space get YUV(NV12 or NV21) */
    } else {
        g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].depth = 0;
    }

    if (g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_YUYV) {
        g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    } else if (g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_NV12) {
        g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].pixel_format = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    } else if (g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_NV21) {
        g_media_cfg[dev_no].vpss_chn_attr[vpss_chn].pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    ret = sampe_uvc_start_vi_vpss_venc(&g_media_cfg[dev_no]);
    if (ret != TD_SUCCESS) {
        sample_print("start vi-vpss-venc failed!\n");
        return ret;
    }

    return TD_SUCCESS;
}

uint16_t venc_backlight_compensation_get(uint32_t dev_no)
{
    return 0;
}

uint16_t venc_brightness_get(uint32_t dev_no)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);

    return (uint16_t)cscf_attr.luma;
}

uint16_t venc_contrast_get(uint32_t dev_no)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);

    return (uint16_t)cscf_attr.contr;
}

uint16_t venc_gain_get(uint32_t dev_no)
{
    return 0;
}

uint16_t venc_hue_get(uint32_t dev_no)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);

    return (uint16_t)cscf_attr.hue;
}

uint8_t venc_power_line_frequency_get(uint32_t dev_no)
{
    ot_isp_exposure_attr exp_attr;

    ss_mpi_isp_get_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);

    if ((exp_attr.auto_attr.antiflicker.frequency != 50) &&   /* 50: frequency */
        (exp_attr.auto_attr.antiflicker.frequency != 60)) {   /* 60: frequency */
        return 0;
    }

    return (exp_attr.auto_attr.antiflicker.frequency == 50) ? 0x1 : 0x2;    /* 50: frequency */
}

uint16_t venc_saturation_get(uint32_t dev_no)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);

    return (uint16_t)cscf_attr.satu;
}

uint16_t venc_sharpness_get(uint32_t dev_no)
{
    return 0;
}

uint16_t venc_gamma_get(uint32_t dev_no)
{
    return 0;
}

uint8_t venc_white_balance_temperature_auto_get(uint32_t dev_no)
{
    ot_isp_wb_attr wb_attr;

    ss_mpi_isp_get_wb_attr((ot_vi_pipe)dev_no, &wb_attr);

    return (wb_attr.op_type == OT_OP_MODE_AUTO) ? 0x1 : 0x0;
}

uint16_t venc_white_balance_temperature_get(uint32_t dev_no)
{
    ot_isp_wb_info wb_info;

    ss_mpi_isp_query_wb_info((ot_vi_pipe)dev_no, &wb_info);

    return (uint16_t)wb_info.color_temp;
}

uint32_t venc_white_balance_component_get(uint32_t dev_no)
{
    return 0;
}

uint8_t venc_white_balance_component_auto_get(uint32_t dev_no)
{
    return 0;
}

uint16_t venc_digital_multiplier_get(uint32_t dev_no)
{
    return 0;
}

uint16_t venc_digital_multiplier_limit_get(uint32_t dev_no)
{
    return 0;
}

uint8_t venc_hue_auto_get(uint32_t dev_no)
{
    return 0;
}

uint8_t venc_analog_video_standard_get(uint32_t dev_no)
{
    return 0;
}

uint8_t venc_analog_lock_status_get(uint32_t dev_no)
{
    return 0;
}

uint8_t venc_contrast_auto_get(uint32_t dev_no)
{
    return 0;
}

td_void venc_backlight_compensation_set(uint32_t dev_no, uint16_t v)
{
    printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_brightness_set(uint32_t dev_no, uint16_t v)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
    cscf_attr.luma = (td_u8)v;
    ss_mpi_isp_set_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
}

td_void venc_contrast_set(uint32_t dev_no, uint16_t v)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
    cscf_attr.contr = (td_u8)v;
    ss_mpi_isp_set_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
}

td_void venc_gain_set(uint32_t dev_no, uint16_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_hue_set(uint32_t dev_no, uint16_t v)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
    cscf_attr.hue = (td_u8)v;
    ss_mpi_isp_set_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
}

td_void venc_power_line_frequency_set(uint32_t dev_no, uint8_t v)
{
    td_s32 ret;
    ot_isp_exposure_attr exp_attr;

    ss_mpi_isp_get_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);
    if (v == 0) {
        exp_attr.auto_attr.antiflicker.enable = TD_FALSE;
    } else if (v == 1) {
        exp_attr.auto_attr.antiflicker.enable = TD_TRUE;
        exp_attr.auto_attr.antiflicker.frequency = 50;  /* 50: frequency */
    } else if (v == 2) {    /* 2: value */
        exp_attr.auto_attr.antiflicker.enable = TD_TRUE;
        exp_attr.auto_attr.antiflicker.frequency = 60;  /* 60: frequency */
    }

    ret = ss_mpi_isp_set_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);
    if (ret != TD_SUCCESS) {
        rlog("ss_mpi_isp_set_exposure_attr err 0x%x\n", ret);
    }
}

td_void venc_saturation_set(uint32_t dev_no, uint16_t v)
{
    ot_isp_csc_attr cscf_attr;

    ss_mpi_isp_get_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
    cscf_attr.satu = (td_u8)v;
    ss_mpi_isp_set_csc_attr((ot_vi_pipe)dev_no, &cscf_attr);
}

td_void venc_sharpness_set(uint32_t dev_no, uint16_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_gamma_set(uint32_t dev_no, uint16_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_white_balance_temperature_auto_set(uint32_t dev_no, uint8_t v)
{
    ot_isp_wb_attr wb_attr;

    ss_mpi_isp_get_wb_attr((ot_vi_pipe)dev_no, &wb_attr);
    wb_attr.op_type = (v == 1) ? OT_OP_MODE_AUTO : OT_OP_MODE_MANUAL;
    ss_mpi_isp_set_wb_attr((ot_vi_pipe)dev_no, &wb_attr);
}

td_void venc_white_balance_temperature_set(uint32_t dev_no, uint16_t v)
{
    ot_isp_wb_info wb_info;
    ot_isp_wb_attr wb_attr;
    td_u16 color_temp;
    td_u16 awb_gain[4];
    errno_t ret;

    ss_mpi_isp_query_wb_info((ot_vi_pipe)dev_no, &wb_info);
    ss_mpi_isp_get_wb_attr((ot_vi_pipe)dev_no, &wb_attr);

    color_temp = v;
    ss_mpi_isp_cal_gain_by_temp((ot_vi_pipe)dev_no, &wb_attr, color_temp, 0, awb_gain);

    wb_attr.op_type = OT_OP_MODE_MANUAL;
    ret = memcpy_s(&wb_attr.manual_attr, sizeof(wb_attr.manual_attr), awb_gain, sizeof(wb_attr.manual_attr));
    if (ret != EOK) {
        err("memcpy_s awb gain fail %x\n", ret);
    }

    ss_mpi_isp_set_wb_attr((ot_vi_pipe)dev_no, &wb_attr);
}


td_void venc_white_balance_component_set(uint32_t dev_no, uint32_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_white_balance_component_auto_set(uint32_t dev_no, uint8_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_digital_multiplier_set(uint32_t dev_no, uint16_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_digital_multiplier_limit_set(uint32_t dev_no, uint16_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_hue_auto_set(uint32_t dev_no, uint8_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

td_void venc_contrast_auto_set(uint32_t dev_no, uint8_t v)
{
printf("func:%s data:0x%x\n", __func__, v);
}

uint8_t venc_exposure_auto_mode_get(uint32_t dev_no)
{
    ot_isp_exposure_attr exp_attr;

    ss_mpi_isp_get_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);

    return (exp_attr.op_type == OT_OP_MODE_AUTO) ? 0x02 : 0x04;
}

uint32_t venc_exposure_ansolute_time_get(uint32_t dev_no)
{
    ot_isp_exposure_attr exp_attr;

    ss_mpi_isp_get_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);

    return exp_attr.manual_attr.exp_time;
}

uint8_t venc_scanning_mode_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_exposure_auto_priority_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_exposure_relative_time_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_focus_absolute_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_focus_relative_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_focus_sample_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_focus_auto_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_iris_absolute_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_iris_relative_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_zoom_absolute_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_zoom_relative_get(uint32_t dev_no)
{
    return 0x5;
}

uint64_t venc_pantilt_absolute_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_pantilt_relative_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_roll_absolute_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_roll_relative_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_ct_privacy_get(uint32_t dev_no)
{
    return 0x5;
}

td_void venc_ct_window_get(uint32_t dev_no, ct_window_params *params)
{
    memset_s(params, sizeof(ct_window_params), 0x0, sizeof(ct_window_params));
    params->window_top = 0x5;
}

td_void venc_region_of_interest_get(uint32_t dev_no, region_of_interest_params *params)
{
    memset_s(params, sizeof(region_of_interest_params), 0x0, sizeof(region_of_interest_params));
    params->roi_top = 0x5;
}

td_void venc_exposure_auto_mode_set(uint32_t dev_no, uint8_t v)
{
    ot_isp_exposure_attr exp_attr;

    ss_mpi_isp_get_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);
    exp_attr.op_type = (v == 4) ? OT_OP_MODE_MANUAL : OT_OP_MODE_AUTO;  /* 4: value */
    ss_mpi_isp_set_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);
}

td_void venc_exposure_ansolute_time_set(uint32_t dev_no, uint32_t v)
{
    ot_isp_exposure_attr exp_attr;

    ss_mpi_isp_get_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);
    exp_attr.manual_attr.exp_time = v * 100;    /* 100: ratio */
    exp_attr.manual_attr.exp_time_op_type = OT_OP_MODE_MANUAL;
    ss_mpi_isp_set_exposure_attr((ot_vi_pipe)dev_no, &exp_attr);
}

td_void venc_scanning_mode_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_exposure_auto_priority_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_exposure_relative_time_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_focus_absolute_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_focus_relative_set(uint32_t dev_no, uint8_t focus_relative, uint8_t speed)
{
    printf("v:0x%x %x \n", speed, focus_relative);
}

td_void venc_focus_sample_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_focus_auto_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_iris_absolute_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_iris_relative_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_zoom_absolute_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_zoom_relative_set(uint32_t dev_no, uint16_t zoom, uint16_t digital_zoom, uint16_t speed)
{
    printf("v:0x%x %x %x \n", speed, digital_zoom, zoom);
}

td_void venc_pantilt_absolute_set(uint32_t dev_no, uint32_t pan_absolute, uint32_t tilt_absolute)
{
    printf("v:0x%x %x \n", tilt_absolute, pan_absolute);
}

td_void venc_pantilt_relative_set(uint32_t dev_no, uint8_t pan_relative, uint8_t pan_speed, uint8_t tilt_relative,
    uint8_t tilt_speed)
{
    printf("v:0x%x %x %x %x \n", tilt_speed, tilt_relative, pan_speed, pan_relative);
}

td_void venc_roll_absolute_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_roll_relative_set(uint32_t dev_no, uint8_t roll_relative, uint8_t speed)
{
    printf("v:0x%x %x \n", speed, roll_relative);
}

td_void venc_ct_privacy_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x \n", v);
}

td_void venc_ct_window_set(uint32_t dev_no, const ct_window_params *params)
{
    printf("v:0x%x %x %x %x %x %x \n", params->num_step_units, params->num_steps, params->window_right,
        params->window_bottom, params->window_left, params->window_top);
}

td_void venc_region_of_interest_set(uint32_t dev_no, const region_of_interest_params *params)
{
    printf("v:0x%x %x %x %x %x \n", params->auto_controls,
        params->roi_right, params->roi_bottom, params->roi_left, params->roi_top);
}

uint16_t venc_select_layer_get(uint32_t dev_no)
{
    return 0x5;
}

uint64_t venc_profile_toolset_get(uint32_t dev_no)
{
    return 0x5;
}
uint32_t venc_video_resolution_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_min_frame_interval_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_slice_mode_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_rate_ctrl_mode_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_average_bitrate_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_cpb_size_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_peak_bit_rate_get(uint32_t dev_no)
{
    return 0x5;
}

uint64_t venc_quantization_params_get(uint32_t dev_no)
{
    return 0x5;
}

uint32_t venc_sync_ref_frame_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_ltr_buffer_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_ltr_picture_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_ltr_validation_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_level_idc_limit_get(uint32_t dev_no)
{
    return 0x5;
}
uint64_t venc_sei_payloadtype_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_qp_range_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_priority_get(uint32_t dev_no)
{
    return 0x5;
}

uint8_t venc_start_or_stop_layer_get(uint32_t dev_no)
{
    return 0x5;
}

uint16_t venc_error_resiliency_get(uint32_t dev_no)
{
    return 0x5;
}

td_void venc_select_layer_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_profile_toolset_set(uint32_t dev_no, uint16_t profile, uint16_t constrained_toolset, uint16_t settings)
{
    printf("v:0x%x %x %x\n", settings, constrained_toolset, profile);
}

td_void venc_video_resolution_set(uint32_t dev_no, uint16_t width, uint16_t height)
{
    printf("v:0x%x %x\n", height, width);
}

td_void venc_min_frame_interval_set(uint32_t dev_no, uint32_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_slice_mode_set(uint32_t dev_no, uint16_t slice_mode, uint16_t slice_config)
{
    printf("v:0x%x %x\n", slice_config, slice_mode);
}

td_void venc_rate_ctrl_mode_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_average_bitrate_set(uint32_t dev_no, uint32_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_cpb_size_set(uint32_t dev_no, uint32_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_peak_bit_rate_set(uint32_t dev_no, uint32_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_quantization_params_set(uint32_t dev_no, uint16_t qp_prime_i, uint16_t qp_prime_p, uint16_t qp_prime_b)
{
    printf("v:0x%x %x %x\n", qp_prime_b, qp_prime_p, qp_prime_i);
}

td_void venc_sync_ref_frame_set(uint32_t dev_no, uint8_t sync_frame_type, uint8_t sync_frame_interval,
    uint8_t gradual_decode_refresh)
{
    printf("v:0x%x %x %x\n", gradual_decode_refresh, sync_frame_interval, sync_frame_type);
}

td_void venc_ltr_buffer_set(uint32_t dev_no, uint8_t ltr_buffer_size, uint8_t ltr_encoder_control)
{
    printf("v:0x%x %x\n", ltr_buffer_size, ltr_encoder_control);
}

td_void venc_ltr_picture_set(uint32_t dev_no, uint8_t put_at_pos_in_ltr_buffer, uint8_t ltr_mode)
{
    printf("v:0x%x %x\n", ltr_mode, put_at_pos_in_ltr_buffer);
}

td_void venc_ltr_validation_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_level_idc_limit_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_sei_payloadtype_set(uint32_t dev_no, uint64_t v)
{
    printf("v:0x%llx\n", v);
}

td_void venc_qp_range_set(uint32_t dev_no, uint8_t min_qp, uint8_t max_qp)
{
    printf("v:0x%x %x\n", max_qp, min_qp);
}

td_void venc_priority_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_start_or_stop_layer_set(uint32_t dev_no, uint8_t v)
{
    printf("v:0x%x\n", v);
}

td_void venc_error_resiliency_set(uint32_t dev_no, uint16_t v)
{
    printf("v:0x%x\n", v);
}

static void *sample_media_send_data(void *arg)
{
    td_char thread_name[32] = {0};
    td_u32 dev_no = (td_u32)(td_uintptr_t)arg;
    if (dev_no >= OT_UVC_MAX_CHN_NUM) {
        printf("invalid dev NO %u\n", dev_no);
        return TD_NULL;
    }
    snprintf_truncated_s(thread_name, sizeof(thread_name), "uvc media data %u", dev_no);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);
    while (g_is_media_start[dev_no]) {
        sample_venc_get_uvc_send(dev_no);
        usleep(4000); /* 4000 4ms give up CPU */
    }
    return TD_NULL;
}

td_s32 sample_venc_set_idr(td_u32 dev_no)
{
    return ss_mpi_venc_request_idr((ot_vi_pipe)dev_no, TD_TRUE);
}

#ifdef UVC_STILL_IMAGE
static td_s32 snap_start_vpss(td_u32 dev_no, encoder_property *p)
{
    ot_vpss_grp vpss_grp = (ot_vpss_grp)dev_no;
    ot_vpss_chn vpss_chn = 1;
    ot_vpss_chn vpss_ext_chn = OT_VPSS_MAX_PHYS_CHN_NUM + 1;

    ot_vpss_grp_attr vpss_grp_attr;
    td_s32 ret = ss_mpi_vpss_get_grp_attr(vpss_grp, &vpss_grp_attr);
    if (ret != TD_SUCCESS) {
        sample_print("vpss_get_grp_attr(%d) failed %#x\n", vpss_grp, ret);
        return TD_FAILURE;
    }

    ot_vpss_chn_attr vpss_chn_attr;
    sample_comm_vpss_get_default_chn_attr(&vpss_chn_attr);
    vpss_chn_attr.compress_mode = OT_COMPRESS_MODE_NONE;
    vpss_chn_attr.width = p->width;
    vpss_chn_attr.height = p->height;
    vpss_chn_attr.pixel_format =
        (p->format == VIDEO_IMG_FORMAT_YUYV) ? OT_PIXEL_FORMAT_YVU_SEMIPLANAR_422 : OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    if ((ret = ss_mpi_vpss_set_chn_attr(vpss_grp, vpss_chn, &vpss_chn_attr)) != TD_SUCCESS) {
        sample_print("vpss_set_chn_attr(%d, %d) failed %#x\n", vpss_grp, vpss_chn, ret);
        return TD_FAILURE;
    }

    if ((ret = ss_mpi_vpss_enable_chn(vpss_grp, vpss_chn)) != TD_SUCCESS) {
        sample_print("vpss_enable_chn(%d, %d) failed %#x\n", vpss_grp, vpss_chn, ret);
        return TD_FAILURE;
    }
    ot_vpss_ext_chn_attr vpss_ext_chn_attr;
    sample_uvc_get_default_vpss_ext_chn_attr(&vpss_ext_chn_attr);
    vpss_ext_chn_attr.depth = 1;
    vpss_ext_chn_attr.width = p->width;
    vpss_ext_chn_attr.height = p->height;
    vpss_ext_chn_attr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    vpss_ext_chn_attr.bind_chn = vpss_chn;

    if (p->format == VIDEO_IMG_FORMAT_YUYV) {
        vpss_ext_chn_attr.pixel_format = OT_PIXEL_FORMAT_VY1UY0_PACKAGE_422;
    } else if (p->format == VIDEO_IMG_FORMAT_NV21) {
        vpss_ext_chn_attr.pixel_format = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }

    if ((ret = ss_mpi_vpss_set_ext_chn_attr(vpss_grp, vpss_ext_chn, &vpss_ext_chn_attr)) != TD_SUCCESS) {
        sample_print("vpss_set_ext_chn_attr(%d, %d) failed %#x\n", vpss_grp, vpss_ext_chn, ret);
        goto disable_chn;
    }

    if ((ret = ss_mpi_vpss_enable_chn(vpss_grp, vpss_ext_chn)) != TD_SUCCESS) {
        sample_print("vpss_enable_chn(%d, %d) failed %#x\n", vpss_grp, vpss_ext_chn, ret);
        goto disable_chn;
    }
    return TD_SUCCESS;

disable_chn:
    (td_void)ss_mpi_vpss_disable_chn(vpss_grp, vpss_chn);
    return TD_FAILURE;
}

static td_s32 snap_stop_vpss(td_u32 dev_no, encoder_property *p)
{
    ot_vpss_grp vpss_grp = (ot_vpss_grp)dev_no;
    ot_vpss_chn vpss_chn = 1;
    ot_vpss_chn vpss_ext_chn = OT_VPSS_MAX_PHYS_CHN_NUM + 1;

    (td_void)ss_mpi_vpss_disable_chn(vpss_grp, vpss_ext_chn);
    (td_void)ss_mpi_vpss_disable_chn(vpss_grp, vpss_chn);
    return TD_SUCCESS;
}

static td_s32 snap_start_venc(td_u32 dev_no, encoder_property *p)
{
    td_s32 ret;
    ot_vpss_grp vpss_grp = (ot_vpss_grp)dev_no;
    ot_vpss_chn vpss_ext_chn = OT_VPSS_MAX_PHYS_CHN_NUM + 1;
    ot_venc_chn venc_chn = (ot_venc_chn)(dev_no + 2);

    sample_comm_venc_chn_param venc_chn_param;
    sample_uvc_get_default_venc_chn_param(&venc_chn_param);
    venc_chn_param.type = OT_PT_MJPEG;
    venc_chn_param.venc_size.width = p->width;
    venc_chn_param.venc_size.height = p->height;
    venc_chn_param.size = sample_comm_sys_get_pic_enum(&venc_chn_param.venc_size);
    ret = sample_comm_venc_start(venc_chn, &venc_chn_param);
    if (ret != TD_SUCCESS) {
        sample_print("venc start(%d) failed %#x\n", venc_chn, ret);
        return TD_FAILURE;
    }

    ot_mpp_chn src = {.mod_id = OT_ID_VPSS, .dev_id = vpss_grp, .chn_id = vpss_ext_chn};
    ot_mpp_chn dst = {.mod_id = OT_ID_VENC, .dev_id = 0, .chn_id = venc_chn};
    ret = ss_mpi_sys_bind(&src, &dst);
    if (ret != TD_SUCCESS) {
        sample_print("venc start(%d) failed %#x\n", venc_chn, ret);
        goto stop_venc;
    }
    return TD_SUCCESS;

stop_venc:
    (td_void)sample_comm_venc_stop(venc_chn);
    return TD_FAILURE;
}

static td_s32 snap_stop_venc(td_u32 dev_no, encoder_property *p)
{
    ot_vpss_grp vpss_grp = (ot_vpss_grp)dev_no;
    ot_vpss_chn vpss_ext_chn = OT_VPSS_MAX_PHYS_CHN_NUM + 1;
    ot_venc_chn venc_chn = (ot_venc_chn)(dev_no + 2);

    ot_mpp_chn src = {.mod_id = OT_ID_VPSS, .dev_id = vpss_grp, .chn_id = vpss_ext_chn};
    ot_mpp_chn dst = {.mod_id = OT_ID_VENC, .dev_id = 0, .chn_id = venc_chn};
    (td_void)ss_mpi_sys_unbind(&src, &dst);
    (td_void)sample_comm_venc_stop(venc_chn);
    return TD_SUCCESS;
}

typedef struct snap_image_info {
    td_u32  size;
    td_void *virt_addr;
} snap_image_info;

static td_s32 snap_wait_yuv(ot_vpss_grp vpss_grp, ot_vpss_chn vpss_ext_chn, ot_video_frame_info *frame_info)
{
    td_s32 ret;
    td_u32 retry = 0;
    const td_u32 max_times = 100;
    while (retry < max_times) {
        usleep(10 * 1000); /* 10 1000 , sleep 10ms */
        retry++;
        ret = ss_mpi_vpss_get_chn_frame(vpss_grp, vpss_ext_chn, frame_info, 100); /* 100 timeout ms */
        if (ret != TD_SUCCESS) {
            if (ret == OT_ERR_VPSS_UNEXIST || ret == OT_ERR_VPSS_NOT_READY) {
                sample_print("vpss chn not exist or ready\n");
                return TD_FAILURE;
            }
            continue;
        }
        if (ret == TD_SUCCESS) {
            break;
        }
    }
    if (retry >= max_times) {
        sample_print("wait yuv failed\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_s32 snap_get_yuv(td_u32 dev_no, const encoder_property *p, snap_image_info *snap_info)
{
    td_s32 ret = TD_SUCCESS;
    ot_vpss_grp vpss_grp = (ot_vpss_grp)dev_no;
    ot_vpss_chn vpss_ext_chn = OT_VPSS_MAX_PHYS_CHN_NUM + 1;
    ot_video_frame_info frame_info;

    ret = snap_wait_yuv(vpss_grp, vpss_ext_chn, &frame_info);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    td_u32 yuv_size = frame_info.video_frame.width * frame_info.video_frame.height * 3 / 2; /* 3 2, pixel 1.5 bytes */
    if (p->format == VIDEO_IMG_FORMAT_YUYV) {
        yuv_size = frame_info.video_frame.width * frame_info.video_frame.height * 2; /* 2 yuyv one pixel 2 bytes */
    }

    td_void *addr = ss_mpi_sys_mmap_cached(frame_info.video_frame.phys_addr[0], yuv_size);
    if (addr == TD_NULL) {
        sample_print("sys_mmap_cached for yuv failed\n");
        return TD_FAILURE;
    }
    snap_info->virt_addr = malloc(yuv_size);
    if (snap_info->virt_addr == TD_NULL) {
        (td_void)ss_mpi_sys_munmap(addr, yuv_size);
        (td_void)ss_mpi_vpss_release_chn_frame(vpss_grp, vpss_ext_chn, &frame_info);
        sample_print("malloc yuv mem %u bytes failed\n", yuv_size);
        return TD_FAILURE;
    }
    snap_info->size = yuv_size;
    if (memcpy_s(snap_info->virt_addr, yuv_size, addr, yuv_size) != EOK) {
        free(snap_info->virt_addr);
        snap_info->virt_addr = TD_NULL;
        sample_print("memcpy_s failed\n");
    }
    (td_void)ss_mpi_sys_flush_cache(frame_info.video_frame.phys_addr[0], addr, yuv_size);
    ss_mpi_sys_munmap(addr, yuv_size);

    /* release frame after using */
    ret = ss_mpi_vpss_release_chn_frame(vpss_grp, vpss_ext_chn, &frame_info);
    if (ret != TD_SUCCESS) {
        if (ret == OT_ERR_VPSS_UNEXIST || ret == OT_ERR_VPSS_NOT_READY) {
            return TD_SUCCESS;
        }
        sample_print("mpi_vpss_release_chn_frame failed %#x\n", ret);
    }
    return ret;
}

static td_s32 snap_wait_jpeg(ot_venc_chn venc_chn, ot_venc_chn_status *stat)
{
    td_s32 ret;
    td_u32 retry = 0;
    const td_u32 max_times = 100;
    while (retry < max_times) {
        usleep(10 * 1000); /* 10 1000 , sleep 10ms */
        retry++;
        ret = ss_mpi_venc_query_status(venc_chn, stat);
        if (ret == OT_ERR_VENC_UNEXIST) {
            sample_print("venc chn %d not exist\n", venc_chn);
            return TD_FAILURE;
        } else if (ret != TD_SUCCESS) {
            continue;
        }

        if (stat->cur_packs != 0) {
            break;
        }
    }
    if (retry >= max_times) {
        sample_print("wait jpeg failed\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_s32 snap_get_jpeg(td_u32 dev_no, encoder_property *p, snap_image_info *snap_info)
{
    td_s32 ret;
    ot_venc_chn venc_chn = (ot_venc_chn)(dev_no + 2);
    ot_venc_stream stream = {0};
    ot_venc_chn_status stat;

    ret = snap_wait_jpeg(venc_chn, &stat);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    stream.pack = (ot_venc_pack *)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
    if (stream.pack == TD_NULL) {
        sample_print("malloc stream pack failed!\n");
        return TD_FAILURE;
    }

    stream.pack_cnt = stat.cur_packs;
    ret = ss_mpi_venc_get_stream(venc_chn, &stream, 100); /* get stream timeout 100 ms */
    if (ret != TD_SUCCESS) {
        sample_print("venc_get_stream failed with %#x!\n", ret);
        goto end;
    }

    /* got JPEG */
    snap_info->size = 0;
    for (td_u32 i = 0; i < stream.pack_cnt; i++) {
        snap_info->size += stream.pack[i].len - stream.pack[i].offset;
    }
    snap_info->virt_addr = malloc(snap_info->size + 64); /* 64 for reserved space */
    if (snap_info->virt_addr == TD_NULL) {
        (td_void)ss_mpi_venc_release_stream(venc_chn, &stream);
        sample_print("malloc jpeg mem %u bytes failed\n", snap_info->size);
        goto end;
    }
    size_t offset = 0;
    for (td_u32 i = 0; i < stream.pack_cnt; i++) {
        td_void *addr = stream.pack[i].addr + stream.pack[i].offset;
        size_t len = stream.pack[i].len - stream.pack[i].offset;
        if (memcpy_s(snap_info->virt_addr + offset, snap_info->size - offset, addr, len) != EOK) {
            free(snap_info->virt_addr);
            sample_print("memcpy_s failed\n");
            break;
        }
        offset += len;
    }

    ret = ss_mpi_venc_release_stream(venc_chn, &stream);
    if (ret != TD_SUCCESS) {
        sample_print("venc_release_stream failed %x\n", ret);
    }

end:
    free(stream.pack);
    return ret;
}

static td_s32 sample_media_snap(td_u32 dev_no, encoder_property *p, snap_image_info *snap_info)
{
    td_s32 ret;
    ret = snap_start_vpss(dev_no, p);
    if (ret != TD_SUCCESS) {
        sample_print("snap_start_vpss failed\n");
        return TD_FAILURE;
    }

    if (p->format == VIDEO_IMG_FORMAT_MJPEG) {
        ret = snap_start_venc(dev_no, p);
        if (ret != TD_SUCCESS) {
            sample_print("snap_start_vpss failed\n");
            goto stop_vpss;
        }
        snap_get_jpeg(dev_no, p, snap_info);
        snap_stop_venc(dev_no, p);
    } else {
        snap_get_yuv(dev_no, p, snap_info);
    }
stop_vpss:
    snap_stop_vpss(dev_no, p);

    return TD_SUCCESS;
}

static td_u32 snap_check_fmt(struct video_framebuffer *still_image)
{
    td_u32 fmts[] = {
        VIDEO_IMG_FORMAT_YUYV,
        VIDEO_IMG_FORMAT_NV12,
        VIDEO_IMG_FORMAT_NV21,
        VIDEO_IMG_FORMAT_MJPEG,
    };
    for (size_t i = 0; i < (sizeof(fmts) / sizeof(fmts[0])); i++) {
        if (still_image->fmt.pixelformat == fmts[i]) {
            return still_image->fmt.pixelformat;
        }
    }
    return TD_NULL;
}

td_s32 sample_get_still_image(struct video_framebuffer *still_image)
{
    encoder_property property = {0};
    property.format = snap_check_fmt(still_image);
    if (property.format == TD_NULL) {
        sample_print("unsupported fmt\n");
        return TD_FAILURE;
    }
    property.width = still_image->fmt.width;
    property.height = still_image->fmt.height;
    printf("snap begin\n");
    snap_image_info snap_info = {0};
    sample_media_snap(0,  &property, &snap_info);
    still_image->base = snap_info.virt_addr;
    still_image->fmt.sizeimage = snap_info.size;
    printf("snap end\n");
    return TD_SUCCESS;
}

#endif

td_s32 sample_venc_init(td_void)
{
    td_s32 ret;
    sample_sns_type sns_type = sample_get_sns_type(0);

    ret = sample_uvc_sys_init(sns_type, g_media_cfg[0].supplement_cfg);
    if (ret != TD_SUCCESS) {
        sample_print("sys init failed!\n");
        return ret;
    }
    sample_venc_set_one_stream_buf();

    return TD_SUCCESS;
}

td_s32 sample_venc_deinit(td_void)
{
    sample_comm_sys_exit();
    return TD_SUCCESS;
}

td_s32 sample_venc_startup(td_u32 dev_no)
{
    td_s32 ret;

    g_is_media_start[dev_no] = TD_TRUE;
    ret = sample_uvc_media_route(dev_no);
    if (g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_H264
        || g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_H265) {
        (td_void)ss_mpi_venc_request_idr(g_media_cfg[dev_no].venc_chn, TD_TRUE);
    }
    if (ret != TD_SUCCESS) {
        g_is_media_start[dev_no] = TD_FALSE;
        return TD_FAILURE;
    }
    ret = pthread_create(&g_thread_send_data[dev_no], TD_NULL, sample_media_send_data, (void *)(td_uintptr_t)dev_no);
    if (ret != TD_SUCCESS) {
        g_is_media_start[dev_no] = TD_FALSE;
        sample_uvc_stop_vi_vpss_venc(&g_media_cfg[dev_no]);
        printf("create send data thread failed\n");
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

td_s32 sample_venc_shutdown(td_u32 dev_no)
{
    if (g_is_media_start[dev_no] == TD_FALSE) {
        return 0;
    }

    g_is_media_start[dev_no] = TD_FALSE;
    pthread_join(g_thread_send_data[dev_no], TD_NULL);
    sample_uvc_stop_vi_vpss_venc(&g_media_cfg[dev_no]);

    return 0;
}

td_s32 sample_venc_set_property(td_u32 dev_no, encoder_property *p)
{
    if (p == NULL) {
        err("p is NULL\n");
        return TD_FAILURE;
    }
    (td_void)memcpy_s(&g_encoder_property[dev_no], sizeof(encoder_property), p, sizeof(encoder_property));
    return 0;
}

td_void sample_uvc_get_encoder_property(td_u32 dev_no, encoder_property *p)
{
    if (p == NULL) {
        err("p is NULL\n");
        return;
    }
    (td_void)memcpy_s(p, sizeof(encoder_property), &g_encoder_property[dev_no], sizeof(encoder_property));
}

static td_u32 uvc_get_yuv_size(const ot_video_frame_info *frame_info)
{
    td_u32 width = frame_info->video_frame.width;
    td_u32 height = frame_info->video_frame.height;

    if ((frame_info->video_frame.pixel_format == OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420) ||
        (frame_info->video_frame.pixel_format == OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420)) {
        return (width * height * 3) / 2; /* y+c is width * 3 / 2 when yuv420 */
    } else {
        return width * height * 2; /* y+c is width * 2 when yuv422 */
    }
}

static td_void sample_uvc_send_yuv(td_u32 dev_no, ot_video_frame_info *frame_info)
{
    td_s32 ret;
    uvc_cache_t *uvc_cache = TD_NULL;
    frame_node_t *fnode = TD_NULL;
    td_void *yuv_data = TD_NULL;
    td_u32 yuv_size = uvc_get_yuv_size(frame_info);

    /* get free cache node */
    uvc_cache = uvc_cache_get(dev_no);
    if (uvc_cache != TD_NULL) {
        get_node_from_queue(uvc_cache->free_queue, &fnode);
    }

    if (fnode == TD_NULL) {
        goto err;
    }
    yuv_data = (td_char *)ss_mpi_sys_mmap_cached(frame_info->video_frame.phys_addr[0], yuv_size);
    if (yuv_data == TD_NULL) {
        goto err;
    }

    /* COPY YUV data */
    ret = memcpy_s(fnode->mem, yuv_size, yuv_data, yuv_size);
    if (ret != TD_SUCCESS) {
        goto err;
    }
    fnode->used = yuv_size;
    (td_void)ss_mpi_sys_flush_cache(frame_info->video_frame.phys_addr[0], yuv_data, yuv_size);
    ss_mpi_sys_munmap(yuv_data, yuv_size);
    put_node_to_queue(uvc_cache->ok_queue, fnode);
    return;
err:
    if (fnode != TD_NULL) {
        put_node_to_queue(uvc_cache->free_queue, fnode);
    }

    if (yuv_data != TD_NULL) {
        ss_mpi_sys_munmap(yuv_data, yuv_size);
        yuv_data = TD_NULL;
    }
    return;
}

static td_s32 sample_send_yuv_frame(td_u32 dev_no)
{
    td_s32 ret = TD_SUCCESS;
    ot_video_frame_info frame_info;
    ot_vpss_grp vpss_grp = (ot_vpss_grp)dev_no;
    ot_vpss_chn vpss_chn = g_media_cfg[dev_no].vpss_chn;

    if (g_encoder_property[dev_no].format == VIDEO_IMG_FORMAT_YUYV) {
        vpss_chn = g_media_cfg[dev_no].vpss_ext_chn;
    }
    ret = ss_mpi_vpss_get_chn_frame(vpss_grp, vpss_chn, &frame_info, 100); /* 100 timeout ms */
    if (ret != TD_SUCCESS) {
        if (ret == OT_ERR_VPSS_UNEXIST || ret == OT_ERR_VPSS_NOT_READY || ret == OT_ERR_VPSS_BUF_EMPTY) {
            return TD_SUCCESS;
        }
        printf("get frame from VPSS %d fail(0x%x)!\n", vpss_chn, ret);
        return TD_FAILURE;
    }

    sample_uvc_send_yuv(dev_no, &frame_info);

    /* release frame after using */
    ret = ss_mpi_vpss_release_chn_frame(vpss_grp, vpss_chn, &frame_info);
    if (ret != TD_SUCCESS) {
        if (ret == OT_ERR_VPSS_UNEXIST || ret == OT_ERR_VPSS_NOT_READY) {
            return TD_SUCCESS;
        }
        sample_print("mpi_vpss_release_chn_frame failed %#x\n", ret);
    }
    return ret;
}

static td_s32 sample_send_venc_stream(td_u32 dev_no, frame_node_t *fnode)
{
    errno_t ret;
    td_u32 i;
    ot_venc_pack *data = TD_NULL;
    unsigned char *s = TD_NULL;
    ot_venc_stream stream = {0};
    ot_venc_chn_status stat;

    ret = ss_mpi_venc_query_status(g_media_cfg[dev_no].venc_chn, &stat);
    if (ret == OT_ERR_VENC_UNEXIST || ret != TD_SUCCESS || stat.cur_packs == 0) {
        return TD_FAILURE;
    }

    stream.pack = (ot_venc_pack *)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
    if (stream.pack == TD_NULL) {
        sample_print("malloc stream pack failed!\n");
        return TD_FAILURE;
    }

    stream.pack_cnt = stat.cur_packs;
    ret = ss_mpi_venc_get_stream(g_media_cfg[dev_no].venc_chn, &stream, 10); /* get stream timeout 10 ms */
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_venc_get_stream failed with %#x!\n", ret);
        free(stream.pack);
        return TD_FAILURE;
    }

    for (i = 0; i < stream.pack_cnt; ++i) {
        data = &stream.pack[i];
        s = data->addr + data->offset;
        td_u32 data_len = data->len - data->offset;
        td_u32 copy_size = data_len < (fnode->length - fnode->used) ? data_len : (fnode->length - fnode->used);

        if (copy_size > 0) {
            ret = memcpy_s(fnode->mem + fnode->used, fnode->length - fnode->used, s, copy_size);
            if (ret != EOK) {
                printf("memcpy_s venc stream fail %#x\n", ret);
            }
            fnode->used += copy_size;
        }
    }

    ret = ss_mpi_venc_release_stream(g_media_cfg[dev_no].venc_chn, &stream);
    if (ret != TD_SUCCESS) {
        sample_print("mpi_venc_release_stream failed %x\n", ret);
    }
    free(stream.pack);
    return TD_SUCCESS;
}


td_s32 sample_venc_get_uvc_send(td_u32 dev_no)
{
    td_s32 ret;

    if (g_is_media_start[dev_no] == TD_FALSE) {
        return TD_SUCCESS;
    }

    if (!sample_uvc_is_need_venc(g_encoder_property[dev_no].format)) {
        return sample_send_yuv_frame(dev_no);
    }

    frame_node_t *fnode = TD_NULL;
    uvc_cache_t *uvc_cache = uvc_cache_get(dev_no);

    if (uvc_cache == TD_NULL) {
        return TD_FAILURE;
    }
    get_node_from_queue(uvc_cache->free_queue, &fnode);
    if (fnode == TD_NULL) {
        return TD_FAILURE;
    }
    fnode->used = 0;

    ret = sample_send_venc_stream(dev_no, fnode);
    if (ret == TD_SUCCESS) {
        put_node_to_queue(uvc_cache->ok_queue, fnode);
    } else {
        put_node_to_queue(uvc_cache->free_queue, fnode);
    }

    return ret;
}

