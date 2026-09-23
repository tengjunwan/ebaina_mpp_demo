/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>

#include "ot_type.h"
#include "sample_comm.h"
#include "media_venc.h"
#include "ot_sns_ctrl.h"
#include "media.h"

static sample_vb_param g_vb_param = {
    /* raw, yuv, vpss chn1 */
    .vb_size = {{2688, 1520}, {2688, 1520}, {CHN1_WIDTH, CHN1_HEIGHT}},
    .pixel_format = {OT_PIXEL_FORMAT_RGB_BAYER_10BPP, OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
        OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420},
    .compress_mode = {OT_COMPRESS_MODE_LINE, OT_COMPRESS_MODE_NONE,
        OT_COMPRESS_MODE_NONE},
    .video_format = {OT_VIDEO_FORMAT_LINEAR, OT_VIDEO_FORMAT_LINEAR,
        OT_VIDEO_FORMAT_LINEAR},
    .blk_num = {2, 3, 1}
};

static sample_comm_venc_chn_param g_venc_chn_param = {
    .frame_rate           = 30, /* 30 is a number */
    .stats_time           = 2,  /* 2 is a number */
    .gop                  = 60, /* 60 is a number */
    .venc_size            = {1920, 1080},
    .size                 = -1,
    .profile              = 0,
    .is_rcn_ref_share_buf = TD_FALSE,
    .gop_attr             = {
        .gop_mode = OT_VENC_GOP_MODE_NORMAL_P,
        .normal_p = {2},
    },
    .type                 = OT_PT_H265,
    .rc_mode              = SAMPLE_RC_AVBR,
};

static kol_route_info g_route_info[MAX_ROUTE_CNT] = {
    {
        .sns_type = SENSOR0_TYPE,
        .vi_pipe = 0,
        .vpss_grp = 0,
        .venc_chn_num = 2,
        .venc_chn = {0},
        .vpss_chn_num = 2
    },
    {
        .sns_type = SENSOR1_TYPE,
        .vi_pipe = 1,
        .vpss_grp = 1,
        .venc_chn_num = 2,
        .venc_chn = {0},
        .vpss_chn_num = 2
    },
    {
        .sns_type = SENSOR0_TYPE,
        .vi_pipe = 2,
        .vpss_grp = 2,
        .venc_chn_num = 2,
        .venc_chn = {0},
        .vpss_chn_num = 2
    },
    {
        .sns_type = SENSOR1_TYPE,
        .vi_pipe = 3,
        .vpss_grp = 3,
        .venc_chn_num = 2,
        .venc_chn = {0},
        .vpss_chn_num = 2
    }
};

static td_u32 g_bus_id[MAX_ROUTE_CNT] = {0, 2, 0, 2};

static sample_vi_cfg g_vi_cfg[MAX_ROUTE_CNT] = {0};

static td_u32 g_route_num = 1;

static ot_vi_vpss_mode_type g_mode_type = OT_VI_OFFLINE_VPSS_ONLINE;

static td_bool g_vi_wrap_enable = TD_TRUE;

static ot_vi_wrap_in_param g_vi_wrap_param = {
    .is_slave = TD_FALSE,
    .pipe_num = 2,
    .wrap_param[0] = {
        .pipe_size = {2560, 1440},
        .frame_rate_100x = 1500,
        .full_lines_std = 1500,
        .slave_param = {
            .vi_vpss_online = TD_TRUE,
            .vpss_chn0_size = {2560, 1440},
            .vpss_venc_wrap = TD_FALSE,
            .large_stream_size = {2560, 1440},
            .small_stream_size = {720, 480},
        }
    },
    .wrap_param[1] = {
        .pipe_size = {2560, 1440},
        .frame_rate_100x = 1500,
        .full_lines_std = 1500,
        .slave_param.vi_vpss_online = TD_TRUE,
        .slave_param.vpss_chn0_size = {2560, 1440},
        .slave_param.vpss_venc_wrap = TD_FALSE,
        .slave_param.large_stream_size = {2560, 1440},
        .slave_param.small_stream_size = {720, 480},
    }
};

static td_bool g_vpss_wrap_enable = TD_TRUE;

static td_u32 g_aidetect_mask = 0;

static td_void media_get_vpss_default_cfg(td_u32 route_id, sample_vpss_cfg *vpss_cfg);
static td_void media_vpss_get_wrap_cfg(td_u32 route_id, sample_vpss_cfg *vpss_cfg);

static td_void media_adjust_vb_blk(td_void)
{
    ot_size size = {0};
    for (td_u32 i = 0; i < g_route_num; i++) {
        ot_size tmp_size;
        sample_comm_vi_get_size_by_sns_type(g_route_info[i].sns_type, &tmp_size);
        if (size.width * size.height < tmp_size.width * tmp_size.height) {
            size = tmp_size;
        }
    }
    g_vb_param.vb_size[0] = g_vb_param.vb_size[1] = size;
    if (g_vi_wrap_enable == TD_TRUE) {
        g_vb_param.blk_num[0] = 0;
    }
    if (g_vpss_wrap_enable == TD_TRUE) {
        g_vb_param.blk_num[1] = 0;
    }
    for (td_u32 i = 0; i < SAMPLE_VIE_POOL_NUM; i++) {
        if (g_route_num > 1) {
            g_vb_param.blk_num[i] *= 2; /* 2/4 routes */
        }
    }
    for (td_u32 i = 0; i < g_route_num; i++) {
        if ((g_aidetect_mask & (1 << i)) != 0) {
            g_vb_param.blk_num[2] += 2;    /* 2: vpss chn depth 1*vb + ai detect 1*vb */
        }
    }
}

static td_s32 media_sys_init(td_void)
{
    td_s32 ret = TD_SUCCESS;
    ot_vb_cfg vb_cfg;

    media_adjust_vb_blk();

    sample_comm_sys_get_default_vb_cfg(&g_vb_param, &vb_cfg);

    /* prepare vpss wrap vb */
    if (g_vpss_wrap_enable == TD_TRUE) {
        sample_vpss_cfg vpss_cfg;
        memset_s(&vpss_cfg, sizeof(vpss_cfg), 0, sizeof(vpss_cfg));
        media_get_vpss_default_cfg(0, &vpss_cfg);
        media_vpss_get_wrap_cfg(0, &vpss_cfg);
        vb_cfg.common_pool[3].blk_cnt = 1; /** 3: vpss wrap vb */
        vb_cfg.common_pool[3].blk_size = vpss_cfg.wrap_attr[0].buf_size;
    }

    td_u32 supplement_config = OT_VB_SUPPLEMENT_BNR_MOT_MASK;
    ret = sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_config);
    if (ret != TD_SUCCESS) {
        sample_print("Init vb supplement failed.");
        return ret;
    }

    ot_vi_aiisp_mode aiisp_mode = OT_VI_AIISP_MODE_DEFAULT;
    ret = sample_comm_vi_set_vi_vpss_mode(g_mode_type, aiisp_mode);
    if (ret != TD_SUCCESS) {
        sample_print("Set vi vpss mode failed.");
        sample_comm_sys_exit();
        return ret;
    }

    return ret;
}

static td_void media_sys_deinit(td_void)
{
    sample_comm_sys_exit();
}

static td_void media_set_vi_cfg(sample_vi_cfg *vi_cfg)
{
    for (td_u32 i = 0; i < g_route_num; i++) {
        sample_comm_vi_get_default_vi_cfg(g_route_info[i].sns_type, &vi_cfg[i]);
        sample_comm_vi_get_mipi_info_by_dev_id(g_route_info[i].sns_type, (td_s32)i, &vi_cfg[i].mipi_info);
        vi_cfg[i].sns_info.bus_id = g_route_num < MAX_ROUTE_CNT ? i : g_bus_id[i];
        vi_cfg[i].sns_info.sns_clk_src = i < OT_VI_MAX_DEV_NUM ? i : i - OT_VI_MAX_DEV_NUM;
        vi_cfg[i].sns_info.sns_rst_src = i < OT_VI_MAX_DEV_NUM ? i : i - OT_VI_MAX_DEV_NUM;
        vi_cfg[i].dev_info.vi_dev = (td_s32)(i < OT_VI_MAX_DEV_NUM ? i : i - OT_VI_MAX_DEV_NUM);
        vi_cfg[i].bind_pipe.pipe_id[0] = (td_s32)i;
        vi_cfg[i].grp_info.grp_num = 0;
        vi_cfg[i].pipe_info[0].isp_info.isp_pub_attr.frame_rate = FRAME_RATE;
        vi_cfg[i].pipe_info[0].isp_be_end_trigger = TD_TRUE;
    }

    for (td_u32 i = OT_VI_MAX_DEV_NUM; i < g_route_num; i++) {
        vi_cfg[i].sns_info.sns_clk_rst_en = TD_FALSE;
        vi_cfg[i].pipe_info[0].pipe_attr.pipe_bypass_mode = OT_VI_PIPE_BYPASS_FE;
        vi_cfg[i - OT_VI_MAX_DEV_NUM].pipe_info[0].set_early_end_mode = TD_TRUE;
        vi_cfg[i - OT_VI_MAX_DEV_NUM].grp_info.distribute_grp_num = 1;
        vi_cfg[i - OT_VI_MAX_DEV_NUM].grp_info.distribute_grp[0] = (td_s32)(i - OT_VI_MAX_DEV_NUM);
        vi_cfg[i - OT_VI_MAX_DEV_NUM].grp_info.distribute_grp_attr[0].distribute_en = TD_TRUE;
        vi_cfg[i - OT_VI_MAX_DEV_NUM].grp_info.distribute_grp_attr[0].pipe_id[0] = (td_s32)(i - OT_VI_MAX_DEV_NUM);
        vi_cfg[i - OT_VI_MAX_DEV_NUM].grp_info.distribute_grp_attr[0].pipe_id[1] = (td_s32)i;
    }
}

static td_void media_set_pipe_wrap_line(sample_vi_cfg vi_cfg[], td_u32 route_num)
{
    ot_vi_wrap_out_param out_param;
    ot_vi_wrap_in_param in_param;
    td_u32 i;

    if (g_vi_wrap_enable == TD_FALSE) {
        return;
    }
    (td_void)memcpy_s(&in_param, sizeof(ot_vi_wrap_in_param), &g_vi_wrap_param, sizeof(ot_vi_wrap_in_param));
    in_param.pipe_num = route_num;
    for (i = 0; i < route_num; i++) {
        sample_comm_vi_get_size_by_sns_type(g_route_info[i].sns_type, &in_param.wrap_param[i].pipe_size);
        sample_comm_vi_get_size_by_sns_type(g_route_info[i].sns_type,
            &in_param.wrap_param[i].slave_param.vpss_chn0_size);
        sample_comm_vi_get_size_by_sns_type(g_route_info[i].sns_type,
            &in_param.wrap_param[i].slave_param.large_stream_size);
        in_param.wrap_param[i].full_lines_std = in_param.wrap_param[i].pipe_size.height;
    }

    if (ss_mpi_sys_get_vi_wrap_buffer_line(&in_param, &out_param) != TD_SUCCESS) {
        sample_print("sample_kol get wrap line fail, pipe_wrap not enable!\n");
        return;
    }

    for (i = 0; i < route_num; i++) {
        vi_cfg[i].pipe_info[0].wrap_attr.buf_line = out_param.buf_line[i];
        vi_cfg[i].pipe_info[0].wrap_attr.enable = TD_TRUE;
    }
}

static td_s32 media_set_ae_stat_pos(td_u32 route_id)
{
    ot_isp_sns_obj *sns_obj = sample_comm_isp_get_sns_obj(g_route_info[route_id].sns_type);
    if (sns_obj == TD_NULL) {
        sample_print("sensor %d not exist!\n", g_route_info[route_id].sns_type);
        return TD_FAILURE;
    }
    if (sns_obj->pfn_set_init == TD_NULL) {
        sample_print("pfn_set_init null!\n");
        return TD_FAILURE;
    }
    ot_isp_init_attr init_attr = {0};
    init_attr.ae_stat_pos = 1;  /* set ae, use be */
    sns_obj->pfn_set_init(g_route_info[route_id].vi_pipe, &init_attr);

    return TD_SUCCESS;
}

static td_s32 media_vi_init(td_void)
{
    td_s32 ret = TD_SUCCESS;
    media_set_vi_cfg(g_vi_cfg);

    media_set_pipe_wrap_line(g_vi_cfg, g_route_num);

    for (td_u32 i = 0; i < g_route_num; i++) {
        ret = media_set_ae_stat_pos(i);
        if (ret != TD_SUCCESS) {
            sample_print("set ae stat pos failed.[0x%x]\n", ret);
            return ret;
        }
        if (g_vi_wrap_enable == TD_FALSE && g_route_num != MAX_ROUTE_CNT) {
            // set low delay
            g_vi_cfg[i].pipe_info[0].low_delay_info.enable = TD_TRUE;
            g_vi_cfg[i].pipe_info[0].low_delay_info.line_cnt =
                g_vi_cfg[i].pipe_info[0].pipe_attr.size.height / 3; /* 3: 1/3 */
            g_vi_cfg[i].pipe_info[0].low_delay_info.one_buf_en = TD_FALSE;
        }
        if (g_vi_cfg[i].bind_pipe.pipe_id[0] >= OT_VI_MAX_PHYS_PIPE_NUM) {
            ret = sample_comm_vi_start_virt_pipe(&g_vi_cfg[i]);
        } else {
            ret = sample_comm_vi_start_vi(&g_vi_cfg[i]);
        }
        if (ret != TD_SUCCESS) {
            sample_print("start vi failed.[0x%x]\n", ret);
            return ret;
        }
    }

    return ret;
}

static td_void media_vi_deinit(td_void)
{
    for (td_u32 i = 0; i < g_route_num; i++) {
        if (g_vi_cfg[i].bind_pipe.pipe_id[0] >= OT_VI_MAX_PHYS_PIPE_NUM) {
            sample_comm_vi_stop_virt_pipe(&g_vi_cfg[i]);
        } else {
            sample_comm_vi_stop_vi(&g_vi_cfg[i]);
        }
    }
}

static td_void media_get_vpss_default_cfg(td_u32 route_id, sample_vpss_cfg *vpss_cfg)
{
    ot_size in_size;
    sample_comm_vi_get_size_by_sns_type(g_route_info[route_id].sns_type, &in_size);
    sample_comm_vpss_get_default_grp_attr(&vpss_cfg->grp_attr);

    vpss_cfg->grp_attr.max_width = in_size.width;
    vpss_cfg->grp_attr.max_height = in_size.height;

    for (td_u32 chn = 0; chn < g_route_info[route_id].vpss_chn_num; chn++) {
        sample_comm_vpss_get_default_chn_attr(&vpss_cfg->chn_attr[chn]);
        vpss_cfg->chn_attr[chn].compress_mode = OT_COMPRESS_MODE_NONE;
        vpss_cfg->chn_attr[chn].width = (chn == 0) ? in_size.width : CHN1_WIDTH;
        vpss_cfg->chn_attr[chn].height = (chn == 0) ? in_size.height : CHN1_HEIGHT;
        vpss_cfg->chn_attr[chn].depth = 0;
        vpss_cfg->chn_en[chn] = TD_TRUE;
    }
    if (g_vpss_wrap_enable == TD_TRUE) {
        vpss_cfg->wrap_attr[0].enable = TD_TRUE;
    }
    if ((g_aidetect_mask & (1 << route_id)) != 0) {
        vpss_cfg->chn_attr[1].depth = 1;
    }
}

static td_void media_vpss_get_wrap_cfg(td_u32 route_id, sample_vpss_cfg *vpss_cfg)
{
    td_u32 full_lines_std;

    if (g_vpss_wrap_enable == TD_FALSE) {
        return;
    }

    switch (g_route_info[route_id].sns_type) {
        case HY006_3814_0011_MIPI_4M_30FPS_10BIT:
        case HY006_3814_0011_MIPI_4M_30FPS_10BIT_WDR2TO1:
        case SC4336P_MIPI_4M_30FPS_10BIT:
            full_lines_std = 1500; /* full_lines_std: 1500 */
            break;
        case HY006_3814_0011_MIPI_2M_30FPS_10BIT:
        case SC4336P_MIPI_2M_30FPS_10BIT:
            full_lines_std = 1125; /* full_lines_std: 1125 */
            break;
        default:
            g_vpss_wrap_enable = TD_FALSE;
            vpss_cfg->wrap_attr[0].enable = TD_FALSE;
            sample_print("sensor type %d not found\n", g_route_info[route_id].sns_type);
            return;
    }
    sample_comm_vpss_get_wrap_cfg(vpss_cfg->chn_attr, g_mode_type, full_lines_std, &vpss_cfg->wrap_attr[0]);
}

static td_s32 media_vpss_init(td_void)
{
    td_s32 ret = TD_SUCCESS;
    sample_vpss_cfg vpss_cfg;
    ot_vpss_svc_param vpss_svc_param;

    for (td_u32 i = 0; i < g_route_num; i++) {
        memset_s(&vpss_cfg, sizeof(vpss_cfg), 0, sizeof(vpss_cfg));
        media_get_vpss_default_cfg(i, &vpss_cfg);
        media_vpss_get_wrap_cfg(i, &vpss_cfg);
        sample_comm_vi_bind_vpss((td_s32)i, 0, (td_s32)i, 0);
        sample_vpss_chn_attr vpss_chn_attr = {0};
        (td_void)memcpy_s(&vpss_chn_attr.chn_attr[0], sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
            vpss_cfg.chn_attr, sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
        (td_void)memcpy_s(&vpss_chn_attr.wrap_attr[0], sizeof(ot_vpss_chn_buf_wrap_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
            vpss_cfg.wrap_attr, sizeof(ot_vpss_chn_buf_wrap_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
        (td_void)memcpy_s(vpss_chn_attr.chn_enable, sizeof(vpss_chn_attr.chn_enable),
            vpss_cfg.chn_en, sizeof(vpss_chn_attr.chn_enable));
        vpss_chn_attr.chn_array_size = OT_VPSS_MAX_PHYS_CHN_NUM;
        ret = sample_common_vpss_start((td_s32)i, &vpss_cfg.grp_attr, &vpss_chn_attr);
        if (ret != TD_SUCCESS) {
            sample_print("vpss start failed.\n");
            return ret;
        }
        ret = ss_mpi_vpss_get_grp_svc_param((td_s32)i, &vpss_svc_param);
        if (ret != TD_SUCCESS) {
            sample_print("get vpss svc param failed.\n");
            return ret;
        }
        vpss_svc_param.enable = 1;
        vpss_svc_param.svc_interval = 3; /** 3: svc interval */
        vpss_svc_param.svc_chn = 1;
        ret = ss_mpi_vpss_set_grp_svc_param((td_s32)i, &vpss_svc_param);
        if (ret != TD_SUCCESS) {
            sample_print("set vpss svc param failed.\n");
            return ret;
        }
    }

    return ret;
}

static td_void media_vpss_deinit(td_void)
{
    td_bool chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_TRUE, TD_FALSE};
    for (td_s32 i = 0; i < (td_s32)g_route_num; i++) {
        sample_common_vpss_stop(i, chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
        sample_comm_vi_un_bind_vpss(i, 0, i, 0);
    }
}

static td_s32 media_venc_jpeg_create(ot_venc_chn venc_chn)
{
    td_s32 ret;
    ot_venc_chn_attr venc_chn_attr;

    venc_chn_attr.venc_attr.type = OT_PT_JPEG;
    venc_chn_attr.venc_attr.max_pic_width = CHN1_WIDTH;
    venc_chn_attr.venc_attr.max_pic_height = CHN1_HEIGHT;
    venc_chn_attr.venc_attr.pic_width = CHN1_WIDTH;
    venc_chn_attr.venc_attr.pic_height = CHN1_HEIGHT;
    venc_chn_attr.venc_attr.buf_size =
        OT_ALIGN_UP(CHN1_WIDTH, 16) * OT_ALIGN_UP(CHN1_HEIGHT, 16) * 3 / 2; /* 16 3 2 is a number */
    venc_chn_attr.venc_attr.profile = 0;
    venc_chn_attr.venc_attr.is_by_frame = TD_TRUE;
    venc_chn_attr.venc_attr.jpeg_attr.dcf_en = TD_FALSE;
    venc_chn_attr.venc_attr.jpeg_attr.mpf_cfg.large_thumbnail_num = 0;
    venc_chn_attr.venc_attr.jpeg_attr.recv_mode = OT_VENC_PIC_RECV_SINGLE;

    venc_chn_attr.gop_attr.gop_mode = OT_VENC_GOP_MODE_NORMAL_P;
    venc_chn_attr.gop_attr.normal_p.ip_qp_delta = 0;

    if ((ret = ss_mpi_venc_create_chn(venc_chn, &venc_chn_attr)) != TD_SUCCESS) {
        sample_print("ss_mpi_venc_create_chn [%d] failed with %#x! ===\n", venc_chn, ret);
        return ret;
    }

    return TD_SUCCESS;
}

static td_s32 venc_start(td_u32 chn_index, td_s32 chn_num, ot_size in_size)
{
    td_s32 ret;
    if (chn_index == 0) {
        g_venc_chn_param.venc_size.width = in_size.width;
        g_venc_chn_param.venc_size.height = in_size.height;
        g_venc_chn_param.type = OT_PT_H265;
        g_venc_chn_param.is_rcn_ref_share_buf = g_vpss_wrap_enable;
        ret = sample_comm_venc_start(chn_num, &g_venc_chn_param);
        if (ret != TD_SUCCESS) {
            sample_print("venc start failed.\n");
            return ret;
        }
        ret = ss_mpi_venc_enable_svc(chn_num, TD_TRUE);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_venc_enable_svc chn[%d] failed with %#x!\n", chn_num, ret);
            return ret;
        }
    } else {
        ret = media_venc_jpeg_create(chn_num);
        if (ret != TD_SUCCESS) {
            sample_print("media_venc_jpeg_create failed with%#x! \n", ret);
            return ret;
        }
    }
    return TD_SUCCESS;
}

static td_s32 media_venc_start(td_void)
{
    td_s32 ret = TD_SUCCESS;
    ot_size in_size;
    td_s32 chn_num = 0;
    for (td_u32 i = 0; i < g_route_num; i++) {
        for (td_u32 j = 0; j < g_route_info[i].venc_chn_num; j++) {
            sample_comm_vi_get_size_by_sns_type(g_route_info[i].sns_type, &in_size);
            ot_venc_chn_config chn_config = {0};

            ret = ss_mpi_venc_get_chn_config(chn_num, &chn_config);
            if (ret != TD_SUCCESS) {
                sample_print("ss_mpi_venc_get_chn_config chn[%d] failed with %#x!\n", chn_num, ret);
                return ret;
            }
            chn_config.svc_version = OT_VENC_SVC_V2;
            ret = ss_mpi_venc_set_chn_config(chn_num, &chn_config);
            if (ret != TD_SUCCESS) {
                sample_print("ss_mpi_venc_set_chn_config chn[%d] failed with %#x!\n", chn_num, ret);
                return ret;
            }
            ret = venc_start(j, chn_num, in_size);
            if (ret != TD_SUCCESS) {
                sample_print("venc start failed with %#x!\n", ret);
                return ret;
            }
            g_route_info[i].venc_chn[j] = chn_num;
            chn_num++;
        }
    }
    return ret;
}

static td_void get_venc_chn(ot_venc_chn *venc_chn, td_s32 *venc_chn_cnt)
{
    td_s32 chn_cnt = 0;
    for (td_u32 i = 0; i < g_route_num; i++) {
        for (td_u32 j = 0; j < g_route_info[i].venc_chn_num; j++) {
            venc_chn[chn_cnt] = g_route_info[i].venc_chn[j];
            chn_cnt++;
        }
    }
    *venc_chn_cnt = chn_cnt;
}

static td_s32 media_venc_init(td_void)
{
    td_s32 ret = TD_SUCCESS;

    ret = media_venc_start();
    if (ret != TD_SUCCESS) {
        sample_print("venc start failed.\n");
        return ret;
    }

    td_s32 venc_cnt = 0;
    ot_venc_chn venc_chn[OT_VENC_MAX_CHN_NUM] = {0};
    get_venc_chn(venc_chn, &venc_cnt);
    ret = media_venc_start_get_stream(venc_chn, venc_cnt);
    if (ret != TD_SUCCESS) {
        media_stop_venc(venc_chn, venc_cnt);
        sample_print("venc start get stream failed.\n");
        return ret;
    }

    for (td_u32 i = 0; i < g_route_num; i++) {
        for (td_u32 j = 0; j < g_route_info[i].venc_chn_num; j++) {
            sample_comm_vpss_bind_venc((td_s32)i, (td_s32)j, g_route_info[i].venc_chn[j]);
        }
    }

    return ret;
}

static td_void media_venc_deinit(td_void)
{
    td_s32 venc_cnt = 0;
    ot_venc_chn venc_chn[OT_VENC_MAX_CHN_NUM] = {0};
    get_venc_chn(venc_chn, &venc_cnt);
    media_stop_venc(venc_chn, venc_cnt);
}

td_s32 media_init(td_void)
{
    td_s32 ret = TD_SUCCESS;

    ret = media_sys_init();
    if (ret != TD_SUCCESS) {
        sample_print("media_sys_init failed.\n");
        goto init_sys_failed;
    }

    ret = media_vi_init();
    if (ret != TD_SUCCESS) {
        sample_print("media_vi_init failed.\n");
        goto init_vi_failed;
    }

    ret = media_vpss_init();
    if (ret != TD_SUCCESS) {
        sample_print("media_vpss_init failed.\n");
        goto init_vpss_failed;
    }

    ret = media_venc_init();
    if (ret != TD_SUCCESS) {
        sample_print("media_venc_init failed.\n");
        goto init_venc_failed;
    }

    return TD_SUCCESS;

init_venc_failed:
    media_venc_deinit();

init_vpss_failed:
    media_vpss_deinit();

init_vi_failed:
    media_vi_deinit();

init_sys_failed:
    sample_comm_sys_exit();

    return ret;
}

td_void media_deinit(td_void)
{
    printf("media deinit.\n");
    media_venc_deinit();
    media_vpss_deinit();
    media_vi_deinit();
    media_sys_deinit();
}

td_u32 media_get_route_num(td_void)
{
    return g_route_num;
}

td_void media_set_route_num(td_u32 route_num)
{
    g_route_num = route_num;
    if (route_num == MAX_ROUTE_CNT) {
        g_vi_wrap_enable = TD_FALSE;
        g_vpss_wrap_enable = TD_FALSE;
    }
}

kol_route_info *media_get_route_info(td_u32 route_id)
{
    if (route_id >= MAX_ROUTE_CNT) {
        return TD_NULL;
    }
    return &g_route_info[route_id];
}

td_void media_set_aidetect_mask(td_u32 mask)
{
    g_aidetect_mask = mask;
}

td_u32 media_get_aidetect_mask(td_void)
{
    return g_aidetect_mask;
}

td_u32 media_get_route_by_venc_chn(ot_venc_chn venc_chn)
{
    for (td_u32 i = 0; i < g_route_num; i++) {
        for (td_u32 j = 0; j < g_route_info[i].venc_chn_num; j++) {
            if (g_route_info[i].venc_chn[j] == venc_chn) {
                return i;
            }
        }
    }
    return 0;
}

td_void media_trigger_jpeg(td_void)
{
    ot_venc_start_param start_param;

    start_param.recv_pic_num = 1;
    for (td_u32 i = 0; i < g_route_num; i++) {
        ss_mpi_venc_start_chn(g_route_info[i].venc_chn[1], &start_param);
    }
}
