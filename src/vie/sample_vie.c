/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h"
#include "sample_ipc.h"
#include "securec.h"
#include "ss_mpi_ae.h"

#include "sns_type.h"
#include "rtsp_server_api.h"

#define X_ALIGN 16
#define Y_ALIGN 2

#define VENC_MAX_CHN_NUM 8

static volatile sig_atomic_t g_sig_flag = 0;

/* this configuration is used to adjust the size and number of buffer(VB).  */
static sample_vb_param g_vb_param = {
    /* raw, yuv, vpss chn1 */
    .vb_size = {{2688, 1520}, {2688, 1520}, {720, 480}},
    .pixel_format = {OT_PIXEL_FORMAT_RGB_BAYER_12BPP, OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
        OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420},
    .compress_mode = {OT_COMPRESS_MODE_LINE, OT_COMPRESS_MODE_NONE,
        OT_COMPRESS_MODE_NONE},
    .video_format = {OT_VIDEO_FORMAT_LINEAR, OT_VIDEO_FORMAT_LINEAR,
        OT_VIDEO_FORMAT_LINEAR},
    .blk_num = {4, 7, 3}
};

static sampe_sys_cfg g_vie_sys_cfg = {
    .route_num = 1,
    .mode_type = OT_VI_OFFLINE_VPSS_OFFLINE,
    .vpss_wrap_en = TD_TRUE,
    .vpss_wrap_size = 3200 * 128 *1.5,
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
    .rc_mode              = SAMPLE_RC_CBR,
};

static sample_vi_fpn_calibration_cfg g_calibration_cfg = {
    .threshold     = 4095, /* 4095 is a number */
    .frame_num     = 16,   /* 16 is a number */
    .fpn_type      = OT_ISP_FPN_TYPE_FRAME,
    .pixel_format  = OT_PIXEL_FORMAT_RGB_BAYER_16BPP,
    .compress_mode = OT_COMPRESS_MODE_NONE,
};

static sample_vi_fpn_correction_cfg g_correction_cfg = {
    .op_mode       = OT_OP_MODE_AUTO,
    .fpn_type      = OT_ISP_FPN_TYPE_FRAME,
    .strength      = 0,
    .pixel_format  = OT_PIXEL_FORMAT_RGB_BAYER_16BPP,
    .compress_mode = OT_COMPRESS_MODE_NONE,
};

static td_void sample_get_char(td_void)
{
    if (g_sig_flag == 1) {
        return;
    }

    sample_pause();
}

/* define SAMPLE_MEM_SHARE_ENABLE, when use tools to dump YUV/RAW. */
#ifdef SAMPLE_MEM_SHARE_ENABLE
td_void sample_vie_init_mem_share(td_void)
{
    td_u32 i;
    ot_vb_common_pools_id pools_id = {0};

    if (ss_mpi_vb_get_common_pool_id(&pools_id) != TD_SUCCESS) {
        sample_print("get common pool_id failed!\n");
        return;
    }
    for (i = 0; i < pools_id.pool_cnt; ++i) {
        ss_mpi_vb_pool_share_all(pools_id.pool[i]);
    }
}
#endif

/* default hy006_3814_0011 x 2 */
static ot_vi_wrap_in_param g_vi_wrap_param = {
    .is_slave = TD_FALSE,
    .pipe_num = 2,
    .wrap_param[0] = {
        .pipe_size = {2560, 1440},
        .frame_rate_100x = 3000,
        .full_lines_std = 1500,
        .slave_param = {
            .vi_vpss_online = TD_TRUE,
            .vpss_chn0_size = {2560, 1440},
            .vpss_venc_wrap = TD_TRUE,
            .large_stream_size = {2560, 1440},
            .small_stream_size = {720, 480},
        }
    },
    .wrap_param[1] = {
        .pipe_size = {2560, 1440},
        .frame_rate_100x = 3000,
        .full_lines_std = 1500,
        .slave_param.vi_vpss_online = TD_TRUE,
        .slave_param.vpss_chn0_size = {2560, 1440},
        .slave_param.vpss_venc_wrap = TD_TRUE,
        .slave_param.large_stream_size = {2560, 1440},
        .slave_param.small_stream_size = {720, 480},
    }
};

static td_void sample_vie_get_pipe_wrap_line(sample_vi_cfg vi_cfg[], td_s32 route_num)
{
    ot_vi_wrap_out_param out_param;
    ot_vi_wrap_in_param in_param;
    td_s32 i;

    if (g_vie_sys_cfg.mode_type == OT_VI_ONLINE_VPSS_ONLINE || g_vie_sys_cfg.mode_type == OT_VI_ONLINE_VPSS_OFFLINE) {
        return;
    }

    (td_void)memcpy_s(&in_param, sizeof(ot_vi_wrap_in_param), &g_vi_wrap_param, sizeof(ot_vi_wrap_in_param));
    if (g_vie_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_OFFLINE) {
        in_param.wrap_param[0].slave_param.vi_vpss_online = TD_FALSE;
        in_param.wrap_param[1].slave_param.vi_vpss_online = TD_FALSE;
    }
    in_param.pipe_num = route_num;
#ifdef OT_FPGA
    in_param.wrap_param[0].frame_rate_100x = 1500; /* 1500: 15fps*100 */
    in_param.wrap_param[1].frame_rate_100x = 1500; /* 1500: 15fps*100 */
    in_param.wrap_param[0].slave_param.vpss_venc_wrap = TD_FALSE;
    in_param.wrap_param[1].slave_param.vpss_venc_wrap = TD_FALSE;
#endif
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(0), &in_param.wrap_param[0].pipe_size);
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(0), &in_param.wrap_param[0].slave_param.vpss_chn0_size);
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(0), &in_param.wrap_param[0].slave_param.large_stream_size);
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(1), &in_param.wrap_param[1].pipe_size);
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(1), &in_param.wrap_param[1].slave_param.vpss_chn0_size);
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(1), &in_param.wrap_param[1].slave_param.large_stream_size);
    if (ss_mpi_sys_get_vi_wrap_buffer_line(&in_param, &out_param) != TD_SUCCESS) {
        sample_print("sample_vie get wrap line fail, pipe_wrap not enable!\n");
        return;
    }

    for (i = 0; i < route_num; i++) {
        vi_cfg[i].pipe_info[0].wrap_attr.buf_line = out_param.buf_line[i];
        vi_cfg[i].pipe_info[0].wrap_attr.enable = TD_TRUE;
    }
}

static td_void sample_vie_vpss_get_default_wrap_buf_size(sample_sns_type sns_type, sampe_sys_cfg *vie_sys_cfg)
{
    ot_size size;
    ot_pic_buf_attr buf_attr = { 0 };
    sample_comm_vi_get_size_by_sns_type(sns_type, &size);

    if (vie_sys_cfg->vpss_wrap_en == TD_FALSE) {
        return;
    }

    buf_attr.width = size.width;
    buf_attr.height = size.height;
    buf_attr.bit_width = OT_DATA_BIT_WIDTH_8;
    buf_attr.pixel_format = OT_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    buf_attr.compress_mode = OT_COMPRESS_MODE_SEG_COMPACT;
    buf_attr.align = OT_DEFAULT_ALIGN;
    buf_attr.video_format = OT_VIDEO_FORMAT_LINEAR;
    vie_sys_cfg->vpss_wrap_size = ot_comm_get_vpss_venc_wrap_buf_size(&buf_attr, 128); // 128: default buf_line
}

static td_s32 sample_vie_sys_init(td_void)
{
    ot_vb_cfg vb_cfg;
    td_u32 supplement_config = OT_VB_SUPPLEMENT_BNR_MOT_MASK | OT_VB_SUPPLEMENT_MOTION_DATA_MASK;

    sample_comm_sys_get_default_vb_cfg(&g_vb_param, &vb_cfg);
    /* prepare vpss wrap vb */
    if (g_vie_sys_cfg.vpss_wrap_en) {
        vb_cfg.common_pool[SAMPLE_VIE_POOL_NUM].blk_cnt = 1;
        vb_cfg.common_pool[SAMPLE_VIE_POOL_NUM].blk_size = g_vie_sys_cfg.vpss_wrap_size;
    }
    if (sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_config) != TD_SUCCESS) {
        return TD_FAILURE;
    }

#ifdef SAMPLE_MEM_SHARE_ENABLE
    sample_vie_init_mem_share();
#endif

    if (sample_comm_vi_set_vi_vpss_mode(g_vie_sys_cfg.mode_type, OT_VI_AIISP_MODE_DEFAULT) != TD_SUCCESS) {
        goto sys_exit;
    }

    return TD_SUCCESS;
sys_exit:
    sample_comm_sys_exit();
    return TD_FAILURE;
}

static td_s32 sample_vie_vpss_set_wrap_grp_int_attr(ot_vi_vpss_mode_type mode_type, td_bool wrap_en, td_u32 max_height)
{
    td_s32 ret;
    if (mode_type == OT_VI_ONLINE_VPSS_ONLINE && wrap_en) {
        ot_frame_interrupt_attr frame_interrupt_attr;
        frame_interrupt_attr.interrupt_type = OT_FRAME_INTERRUPT_EARLY_END;
        frame_interrupt_attr.early_line = max_height / 2; /* 2 half */
        ret = ss_mpi_vpss_set_grp_frame_interrupt_attr(0, &frame_interrupt_attr);
        if (ret != TD_SUCCESS) {
            printf("ss_mpi_vpss_set_grp_frame_interrupt_attr failed!\n");
            return ret;
        }
    }
    return TD_SUCCESS;
}

static td_s32 sample_vie_start_vpss(ot_vpss_grp grp, sample_vpss_cfg *vpss_cfg)
{
    td_s32 ret;
    sample_vpss_chn_attr vpss_chn_attr = {0};

    if (grp == 0) {
        ret = sample_vie_vpss_set_wrap_grp_int_attr(g_vie_sys_cfg.mode_type, vpss_cfg->wrap_attr[0].enable,
            vpss_cfg->grp_attr.max_height);
        if (ret != TD_SUCCESS) {
            return ret;
        }
    }

    (td_void)memcpy_s(&vpss_chn_attr.chn_attr[0], sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
        vpss_cfg->chn_attr, sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
    (td_void)memcpy_s(vpss_chn_attr.chn_enable, sizeof(vpss_chn_attr.chn_enable),
        vpss_cfg->chn_en, sizeof(vpss_chn_attr.chn_enable));
    (td_void)memcpy_s(&vpss_chn_attr.wrap_attr[0], sizeof(ot_vpss_chn_buf_wrap_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
        vpss_cfg->wrap_attr, sizeof(ot_vpss_chn_buf_wrap_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
    vpss_chn_attr.chn_array_size = OT_VPSS_MAX_PHYS_CHN_NUM;
    ret = sample_common_vpss_start(grp, &vpss_cfg->grp_attr, &vpss_chn_attr);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    if (vpss_cfg->chn_en[1]) {
        const ot_low_delay_info low_delay_info = { TD_TRUE, 200, TD_FALSE }; /* 200: lowdelay line */
        if (ss_mpi_vpss_set_chn_low_delay(grp, 1, &low_delay_info) != TD_SUCCESS) {
            goto stop_vpss;
        }
    }

    return TD_SUCCESS;
stop_vpss:
    sample_common_vpss_stop(grp, vpss_cfg->chn_en, OT_VPSS_MAX_PHYS_CHN_NUM);
    return TD_FAILURE;
}

static td_void sample_vie_stop_vpss(ot_vpss_grp grp)
{
    td_bool chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_TRUE, TD_FALSE};

    sample_common_vpss_stop(grp, chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
}

static td_s32 sample_vie_start_venc(ot_venc_chn venc_chn[], ot_size venc_size[], size_t size, td_u32 chn_num)
{
    td_s32 i;
    td_s32 ret;

    if (chn_num > size) {
        return TD_FAILURE;
    }

    for (i = 0; i < (td_s32)chn_num; i++) {
        g_venc_chn_param.venc_size.width = venc_size[i].width;
        g_venc_chn_param.venc_size.height = venc_size[i].height;
        g_venc_chn_param.is_rcn_ref_share_buf = 1;
        ret = sample_comm_venc_start(venc_chn[i], &g_venc_chn_param);
        if (ret != TD_SUCCESS) {
            goto exit;
        }
    }

    ret = sample_comm_venc_start_get_stream(venc_chn, chn_num);
    if (ret != TD_SUCCESS) {
        goto exit;
    }

    return TD_SUCCESS;

exit:
    for (i = i - 1; i >= 0; i--) {
        sample_comm_venc_stop(venc_chn[i]);
    }
    return TD_FAILURE;
}

static td_void sample_vie_stop_venc(ot_venc_chn venc_chn[], size_t size, td_u32 chn_num)
{
    td_u32 i;

    if (chn_num > size) {
        return;
    }

    sample_comm_venc_stop_get_stream(chn_num);

    for (i = 0; i < chn_num; i++) {
        sample_comm_venc_stop(venc_chn[i]);
    }
}

static td_s32 sample_vie_start_and_bind_venc(ot_vpss_grp vpss_grp[], sample_vpss_cfg *vpss_cfg, size_t size,
    td_u32 grp_num)
{
    td_u32 i;
    td_s32 ret;
    ot_venc_chn venc_chn[VENC_MAX_CHN_NUM];
    ot_size venc_size[VENC_MAX_CHN_NUM];
    td_u32 chn_num;

    if (grp_num > size) {
        return TD_FAILURE;
    }

    for (i = 0; i < VENC_MAX_CHN_NUM; i++) {
        venc_chn[i] = i;
        if (i % 2 == 0) { /* 2: divisor */
            venc_size[i].width = vpss_cfg->chn_attr[0].width;
            venc_size[i].height = vpss_cfg->chn_attr[0].height;
        } else {
            venc_size[i].width = vpss_cfg->chn_attr[1].width;
            venc_size[i].height = vpss_cfg->chn_attr[1].height;
        }
    }

    chn_num = grp_num;
    if (g_vie_sys_cfg.vpss_wrap_en) {
        chn_num *= 2; /* 2 chns */
    }
    ret = sample_vie_start_venc(venc_chn, venc_size, sizeof(venc_chn) / sizeof(venc_chn[0]), chn_num);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_bind_venc(vpss_grp[i], OT_VPSS_CHN0, venc_chn[i * 2]); /* 2 chns */
        if (g_vie_sys_cfg.vpss_wrap_en) {
            sample_comm_vpss_bind_venc(vpss_grp[i], OT_VPSS_CHN1, venc_chn[i * 2 + 1]); /* 2 chns */
        }
    }

    return TD_SUCCESS;
}

static td_void sample_vie_stop_and_unbind_venc(ot_vpss_grp vpss_grp[], size_t size, td_u32 grp_num)
{
    td_u32 i;
    ot_venc_chn venc_chn[VENC_MAX_CHN_NUM];
    td_u32 chn_num;

    if (grp_num > size) {
        return;
    }

    for (i = 0; i < VENC_MAX_CHN_NUM; i++) {
        venc_chn[i] = i;
    }

    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_un_bind_venc(vpss_grp[i], OT_VPSS_CHN0, venc_chn[i * 2]); /* 2 chns */
        if (g_vie_sys_cfg.vpss_wrap_en) {
            sample_comm_vpss_un_bind_venc(vpss_grp[i], OT_VPSS_CHN1, venc_chn[i * 2 + 1]); /* 2 chns */
        }
    }

    chn_num = grp_num;
    if (g_vie_sys_cfg.vpss_wrap_en) {
        chn_num *= 2; /* 2 chns */
    }
    sample_vie_stop_venc(venc_chn, sizeof(venc_chn) / sizeof(venc_chn[0]), chn_num);
}

static td_s32 sample_vie_start_route(sample_vi_cfg *vi_cfg, sample_vpss_cfg *vpss_cfg, td_s32 route_num)
{
    td_s32 i, j, ret;
    ot_vpss_grp vpss_grp[SAMPLE_VIE_MAX_ROUTE_NUM] = {0, 1, 2, 3};

    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(0), &g_vb_param.vb_size[0]);
    sample_comm_vi_get_size_by_sns_type(sample_get_sns_type(0), &g_vb_param.vb_size[1]);
    if (sample_vie_sys_init() != TD_SUCCESS) {
        return TD_FAILURE;
    }

    for (i = 0; i < route_num; i++) {
        ret = sample_comm_vi_start_vi(&vi_cfg[i]);
        if (ret != TD_SUCCESS) {
            goto start_vi_failed;
        }
    }

    for (i = 0; i < route_num; i++) {
        sample_comm_vi_bind_vpss(i, 0, vpss_grp[i], 0);
    }

    for (i = 0; i < route_num; i++) {
        ret = sample_vie_start_vpss(vpss_grp[i], vpss_cfg);
        if (ret != TD_SUCCESS) {
            goto start_vpss_failed;
        }
    }

    ret = sample_vie_start_and_bind_venc(vpss_grp, vpss_cfg, SAMPLE_VIE_MAX_ROUTE_NUM, route_num);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }

    return TD_SUCCESS;

start_vpss_failed:
    for (j = i - 1; j >= 0; j--) {
        sample_vie_stop_vpss(vpss_grp[j]);
    }
    for (i = 0; i < route_num; i++) {
        sample_comm_vi_un_bind_vpss(i, 0, vpss_grp[i], 0);
    }
start_vi_failed:
    for (j = i - 1; j >= 0; j--) {
        sample_comm_vi_stop_vi(&vi_cfg[j]);
    }
    sample_comm_sys_exit();
    return TD_FAILURE;
}

static td_void sample_vie_stop_route(sample_vi_cfg *vi_cfg, td_s32 route_num)
{
    td_s32 i;
    ot_vpss_grp vpss_grp[SAMPLE_VIE_MAX_ROUTE_NUM] = {0, 1, 2, 3};

    sample_vie_stop_and_unbind_venc(vpss_grp, SAMPLE_VIE_MAX_ROUTE_NUM, route_num);
    for (i = route_num - 1; i >= 0; i--) {
        sample_vie_stop_vpss(vpss_grp[i]);
        sample_comm_vi_un_bind_vpss(i, 0, vpss_grp[i], 0);
        sample_comm_vi_stop_vi(&vi_cfg[i]);
    }
    sample_comm_sys_exit();
}

static td_void sample_vie_print_vi_mode_list(td_bool is_wdr_mode)
{
    printf("vi vpss mode list: \n");
    if (is_wdr_mode == TD_TRUE) {
        printf("    (0) VI_ONLINE_VPSS_ONLINE\n");
        printf("    (1) VI_ONLINE_VPSS_OFFLINE\n");
        printf("    (2) VI_OFFLINE_VPSS_ONLINE\n");
        printf("    (3) VI_OFFLINE_VPSS_OFFLINE\n");
        printf("please select mode:\n");
        return;
    }
    printf("    (0) VI_ONLINE_VPSS_ONLINE\n");
    printf("    (1) VI_ONLINE_VPSS_OFFLINE\n");
    printf("    (2) VI_OFFLINE_VPSS_ONLINE\n");
    printf("    (3) VI_OFFLINE_VPSS_OFFLINE\n");
    printf("please select mode:\n");
}

static td_void sample_vie_get_vi_vpss_mode_by_char(td_char ch, td_bool is_wdr)
{
    switch (ch) {
        case '0':
            g_vie_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_ONLINE;
            g_vb_param.blk_num[0] = 0; /* raw_vb num 0 */
            g_vb_param.blk_num[1] = 0; /* yuv_vb num 0 */
            break;
        case '1':
            g_vie_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_OFFLINE;
            g_vb_param.blk_num[0] = 0; /* raw_vb num 0 */
            g_vb_param.blk_num[1] = 8; /* yuv_vb num 8 */
            break;
        case '2':
            g_vie_sys_cfg.mode_type = OT_VI_OFFLINE_VPSS_ONLINE;
            g_vb_param.blk_num[0] = is_wdr ? 6 : 0; /* raw_vb num 6 or 0 */
            g_vb_param.blk_num[1] = 0; /* yuv_vb num 0 */
            break;
        case '3':
            g_vie_sys_cfg.mode_type = OT_VI_OFFLINE_VPSS_OFFLINE;
            g_vb_param.blk_num[0] = is_wdr ? 4 : 0; /* raw_vb num 4 or 0 */
            break;
        default:
            g_vie_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_ONLINE;
            g_vb_param.blk_num[0] = 0; /* raw_vb num 0 */
            g_vb_param.blk_num[1] = 0; /* yuv_vb num 0 */
            break;
    }
}

static td_void sample_vie_get_vi_vpss_mode(td_bool is_wdr_mode)
{
    td_char ch = '0';
    td_char end_ch = '3';
    td_char input[3] = {0}; /* max_len: 3 */
    td_s32 max_len = 3; /* max_len: 3 */

    sample_vie_print_vi_mode_list(is_wdr_mode);

    while (g_sig_flag == 0) {
        if (gets_s(input, max_len) != TD_NULL && strlen(input) == 1 && input[0] >= ch && input[0] <= end_ch) {
            break;
        } else {
            printf("\nInvalid param, please enter again!\n\n");
            sample_vie_print_vi_mode_list(is_wdr_mode);
        }
        (td_void)fflush(stdin);
    }

    sample_vie_get_vi_vpss_mode_by_char(input[0], is_wdr_mode);
}

static td_void sample_vie_vpss_get_wrap_cfg(sampe_sys_cfg *vie_sys_cfg, sample_vpss_cfg *vpss_cfg)
{
    td_u32 full_lines_std;
    if (vie_sys_cfg->vpss_wrap_en) {
        if (sample_get_sns_type(0) == HY006_3814_0011_MIPI_4M_30FPS_10BIT ||
            sample_get_sns_type(0) == HY006_3814_0011_MIPI_3M_30FPS_10BIT ||
            sample_get_sns_type(0) == HY006_3814_0011_MIPI_2M_30FPS_10BIT ||
            sample_get_sns_type(0) == HY006_3814_0011_MIPI_4M_30FPS_10BIT_WDR2TO1 ||
            sample_get_sns_type(0) == SC4336P_MIPI_4M_30FPS_10BIT ||
            sample_get_sns_type(0) == SC4336P_MIPI_3M_30FPS_10BIT ||
            sample_get_sns_type(0) == SC4336P_MIPI_2M_30FPS_10BIT) {
            full_lines_std = 1500; /* full_lines_std: 1500 */
        } else if (sample_get_sns_type(0) == SC450AI_MIPI_4M_30FPS_10BIT || sample_get_sns_type(0) == SC450AI_MIPI_4M_30FPS_10BIT_WDR2TO1) {
            full_lines_std = 1585; /* full_lines_std: 1585 */
        } else if (sample_get_sns_type(0) == SC500AI_MIPI_5M_30FPS_10BIT || sample_get_sns_type(0) == SC500AI_MIPI_5M_30FPS_10BIT_WDR2TO1) {
            full_lines_std = 1700; /* full_lines_std: 1700 */
        } else {
            vie_sys_cfg->vpss_wrap_en = TD_FALSE;
            vpss_cfg->wrap_attr[0].enable = TD_FALSE;
            return;
        }
        (td_void)sample_comm_vpss_get_wrap_cfg(vpss_cfg->chn_attr, vie_sys_cfg->mode_type, full_lines_std,
            &vpss_cfg->wrap_attr[0]);
        vie_sys_cfg->vpss_wrap_size = vpss_cfg->wrap_attr[0].buf_size;
    }
}

static td_s32 sample_vie_all_mode(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg;
    sample_sns_type sns_type = sample_get_sns_type(0);

    sample_vie_get_vi_vpss_mode(TD_FALSE);
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_vie_get_pipe_wrap_line(vi_cfg, 1);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);

    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_get_char();

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_s32 sample_vie_wdr(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg;
    sample_sns_type sns_type = sample_get_sns_type(0);

    sample_vie_get_vi_vpss_mode(TD_TRUE);

    /* HY006_3814_0011_MIPI_4M_30FPS_10BIT not support WDR */
    if (sns_type == HY006_3814_0011_MIPI_4M_30FPS_10BIT ||
        sns_type == HY006_3814_0011_MIPI_3M_30FPS_10BIT ||
        sns_type == HY006_3814_0011_MIPI_2M_30FPS_10BIT ||
        sns_type == HY006_3814_0011_MIPI_4M_30FPS_10BIT_WDR2TO1 ||
        sns_type == SC4336P_MIPI_4M_30FPS_10BIT ||
        sns_type == SC4336P_MIPI_3M_30FPS_10BIT ||
        sns_type == SC4336P_MIPI_2M_30FPS_10BIT) {
        sample_print("Not supported by current sensor%d\n", sns_type);
        return TD_FAILURE;
    }

    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    if (g_vie_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_ONLINE || g_vie_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_OFFLINE) {
        vi_cfg[0].pipe_info[0].pipe_attr.compress_mode = OT_COMPRESS_MODE_FRAME;
        vi_cfg[0].pipe_info[1].pipe_attr.compress_mode = OT_COMPRESS_MODE_FRAME;
        g_vb_param.compress_mode[0] = OT_COMPRESS_MODE_FRAME;
    }
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);

    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_get_char();

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_s32 sample_vie_switch_first_route(sample_sns_type sns_type, sample_vi_cfg *vi_cfg, td_bool is_run_be)
{
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vpss_grp vpss_grp[1] = {0};
    const td_u32 grp_num = 1;
    const ot_vpss_chn vpss_chn = 0;
    sample_vpss_cfg vpss_cfg;
    ot_size in_size;
    sample_run_be_bind_pipe bind_pipe = {
        .wdr_mode = OT_WDR_MODE_NONE, .pipe_id = {0}, .pipe_num = 1};
    td_s32 ret;

    sample_comm_vi_get_size_by_sns_type(sns_type, &in_size);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);
    vi_cfg->pipe_info[0].isp_be_end_trigger = is_run_be;
    ret = sample_comm_vi_start_vi(vi_cfg);
    if (ret != TD_SUCCESS) {
        goto start_vi_failed;
    }
    sample_comm_vi_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    ret = sample_vie_start_vpss(vpss_grp[0], &vpss_cfg);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }
    ret = sample_vie_start_and_bind_venc(vpss_grp, &vpss_cfg, sizeof(vpss_grp) / sizeof(vpss_grp[0]), grp_num);
    if (ret != TD_SUCCESS) {
        goto start_venc_failed;
    }

    if (is_run_be) {
        sample_comm_vi_send_run_be_frame(&bind_pipe);
    } else {
        sample_print_pause("press enter key to switch!\n");
    }

    sample_vie_stop_and_unbind_venc(vpss_grp, sizeof(vpss_grp) / sizeof(vpss_grp[0]), grp_num);
start_venc_failed:
    sample_vie_stop_vpss(vpss_grp[0]);
start_vpss_failed:
    sample_comm_vi_un_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    if (ret == TD_SUCCESS) {
        sample_comm_vi_mode_switch_stop_vi(vi_cfg);
    } else {
        sample_comm_vi_stop_vi(vi_cfg);
    }
start_vi_failed:
    return ret;
}

static td_s32 sample_vie_switch_second_route(sample_sns_type sns_type, td_bool is_run_be)
{
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vpss_grp vpss_grp[1] = {0};
    const td_u32 grp_num = 1;
    const ot_vpss_chn vpss_chn = 0;
    ot_size in_size;
    sample_vi_cfg vi_cfg;
    sample_vpss_cfg vpss_cfg;
    sample_run_be_bind_pipe bind_pipe = {
        .wdr_mode = OT_WDR_MODE_2To1_LINE,
        .pipe_id = {0, 1},
        .pipe_num = 2
    };
    td_s32 ret;

    sample_comm_vi_get_size_by_sns_type(sns_type, &in_size);
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);
    ret = sample_comm_vi_mode_switch_start_vi(&vi_cfg, TD_FALSE, &in_size);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_comm_vi_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);

    ret = sample_vie_start_vpss(vpss_grp[0], &vpss_cfg);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }

    ret = sample_vie_start_and_bind_venc(vpss_grp, &vpss_cfg, sizeof(vpss_grp) / sizeof(vpss_grp[0]), grp_num);
    if (ret != TD_SUCCESS) {
        goto start_venc_failed;
    }

    if (is_run_be) {
        sample_comm_vi_send_run_be_frame(&bind_pipe);
    } else {
        sample_get_char();
    }

    sample_vie_stop_and_unbind_venc(vpss_grp, sizeof(vpss_grp) / sizeof(vpss_grp[0]), grp_num);
start_venc_failed:
    sample_vie_stop_vpss(vpss_grp[0]);
start_vpss_failed:
    sample_comm_vi_un_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    sample_comm_vi_stop_vi(&vi_cfg);

    return ret;
}

static td_void sample_vie_vpss_get_cfg_by_size(sample_sns_type sns_type, sample_vpss_cfg *vpss_cfg, ot_size *in_size)
{
    sample_comm_vpss_get_default_vpss_cfg(sns_type, vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    vpss_cfg->grp_attr.max_width = in_size->width;
    vpss_cfg->grp_attr.max_height = in_size->height;
    vpss_cfg->chn_attr[0].width = in_size->width;
    vpss_cfg->chn_attr[0].height = in_size->height;
}

static td_s32 sample_vie_switch_resolution_route(sample_sns_type sns_type)
{
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vpss_grp vpss_grp[1] = {0};
    const td_u32 grp_num = 1;
    const ot_vpss_chn vpss_chn = 0;
    ot_size in_size;
    sample_vi_cfg vi_cfg;
    sample_vpss_cfg vpss_cfg;
    td_s32 ret;

    in_size.width = 1920;  //  switch to 1920
    in_size.height = 1080; //  switch to 1080

    sample_comm_vi_init_vi_cfg(sns_type, &in_size, &vi_cfg);
    sample_vie_get_pipe_wrap_line(&vi_cfg, 1);
    sample_vie_vpss_get_cfg_by_size(sns_type, &vpss_cfg, &in_size);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);

    ret = sample_comm_vi_mode_switch_start_vi(&vi_cfg, TD_TRUE, &in_size);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_comm_vi_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    ret = sample_vie_start_vpss(vpss_grp[0], &vpss_cfg);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }

    ret = sample_vie_start_and_bind_venc(vpss_grp, &vpss_cfg, sizeof(vpss_grp) / sizeof(vpss_grp[0]), grp_num);
    if (ret != TD_SUCCESS) {
        goto start_venc_failed;
    }

    sample_get_char();

    sample_vie_stop_and_unbind_venc(vpss_grp, sizeof(vpss_grp) / sizeof(vpss_grp[0]), grp_num);
start_venc_failed:
    sample_vie_stop_vpss(vpss_grp[0]);
start_vpss_failed:
    sample_comm_vi_un_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    sample_comm_vi_stop_vi(&vi_cfg);

    return ret;
}

static td_s32 sample_vie_switch_mode(td_void)
{
    td_s32 ret;
    sample_sns_type sns_type = sample_get_sns_type(0);
    td_bool is_run_be = TD_FALSE;
    sample_vi_cfg vi_cfg;

    sample_vie_get_vi_vpss_mode(TD_TRUE);
    sample_comm_vi_get_size_by_sns_type(sns_type, &g_vb_param.vb_size[0]);
    sample_comm_vi_get_size_by_sns_type(sns_type, &g_vb_param.vb_size[1]);
    sample_vie_vpss_get_default_wrap_buf_size(sns_type, &g_vie_sys_cfg);
    ret = sample_vie_sys_init();
    if (ret != TD_SUCCESS) {
        return ret;
    }

    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg);
    ret = sample_vie_switch_first_route(sns_type, &vi_cfg, is_run_be);
    if (ret != TD_SUCCESS) {
        sample_comm_sys_exit();
        return ret;
    }

    if (sns_type == SC450AI_MIPI_4M_30FPS_10BIT) {
        sns_type = SC450AI_MIPI_4M_30FPS_10BIT_WDR2TO1;
    } else if (sns_type == SC500AI_MIPI_5M_30FPS_10BIT) {
        sns_type = SC500AI_MIPI_5M_30FPS_10BIT_WDR2TO1;
    } else {
        printf("Not supported by current sensor%d\n", sns_type);
        sample_comm_vi_stop_isp(&vi_cfg);
        sample_comm_sys_exit();
        return TD_FAILURE;
    }

    ret = sample_vie_switch_second_route(sns_type, is_run_be);
    sample_comm_sys_exit();
    return ret;
}

static td_s32 sample_vie_switch_resolution(td_void)
{
    td_s32 ret;

    sample_sns_type sns_type = sample_get_sns_type(0);
    td_bool is_run_be = TD_FALSE;
    sample_vi_cfg vi_cfg;

    sample_vie_get_vi_vpss_mode(TD_FALSE);
    sample_comm_vi_get_size_by_sns_type(sns_type, &g_vb_param.vb_size[0]);
    sample_comm_vi_get_size_by_sns_type(sns_type, &g_vb_param.vb_size[1]);
    sample_vie_vpss_get_default_wrap_buf_size(sns_type, &g_vie_sys_cfg);
    ret = sample_vie_sys_init();
    if (ret != TD_SUCCESS) {
        return ret;
    }

    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg);
    sample_vie_get_pipe_wrap_line(&vi_cfg, 1);
    ret = sample_vie_switch_first_route(sns_type, &vi_cfg, is_run_be);
    if (ret != TD_SUCCESS) {
        sample_comm_sys_exit();
        return ret;
    }

    ret = sample_vie_switch_resolution_route(sns_type);
    sample_comm_sys_exit();
    return ret;
}

static td_s32 sample_vie_run_be_switch_mode(td_void)
{
    td_s32 ret;
    sample_sns_type sns_type = sample_get_sns_type(0);
    td_bool is_run_be = TD_TRUE;
    sample_vi_cfg vi_cfg;

    sample_vie_get_vi_vpss_mode_by_char('2', TD_TRUE);  // '2': VI_OFFLINE_VPSS_ONLINE
    sample_comm_vi_get_size_by_sns_type(sns_type, &g_vb_param.vb_size[0]);
    sample_comm_vi_get_size_by_sns_type(sns_type, &g_vb_param.vb_size[1]);
    sample_vie_vpss_get_default_wrap_buf_size(sns_type, &g_vie_sys_cfg);
    g_vb_param.blk_num[0] = 4; /* raw_vb num 4 */
    g_vb_param.blk_num[1] = 4; /* yuv_vb num 4 */
    ret = sample_vie_sys_init();
    if (ret != TD_SUCCESS) {
        return ret;
    }

    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg);
    ret = sample_vie_switch_first_route(sns_type, &vi_cfg, is_run_be);
    if (ret != TD_SUCCESS) {
        sample_comm_sys_exit();
        return ret;
    }

    if (sns_type == SC450AI_MIPI_4M_30FPS_10BIT) {
        sns_type = SC450AI_MIPI_4M_30FPS_10BIT_WDR2TO1;
    } else if (sns_type == SC500AI_MIPI_5M_30FPS_10BIT) {
        sns_type = SC500AI_MIPI_5M_30FPS_10BIT_WDR2TO1;
    } else {
        printf("Not supported by current sensor%d\n", sns_type);
        sample_comm_vi_stop_isp(&vi_cfg);
        sample_comm_sys_exit();
        return TD_FAILURE;
    }

    ret = sample_vie_switch_second_route(sns_type, is_run_be);
    sample_comm_sys_exit();
    return ret;
}

static td_void sample_vie_do_fpn_calibrate_and_correction(ot_vi_pipe vi_pipe)
{
    sample_comm_vi_fpn_calibrate(vi_pipe, &g_calibration_cfg);

    printf("press enter key to enable fpn correction!\n");
    sample_get_char();

    sample_comm_vi_enable_fpn_correction(vi_pipe, &g_correction_cfg);
}

static td_s32 sample_vie_fpn(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg;
    const ot_vi_pipe vi_pipe = 0;
    g_vie_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_ONLINE;
    g_vb_param.blk_num[0] = 0; /* online unuse raw vb */
    g_vb_param.blk_num[1] = 0; /* online unuse yuv vb  */
    sample_sns_type sns_type = sample_get_sns_type(0);
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);
    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_vie_do_fpn_calibrate_and_correction(vi_pipe);

    sample_get_char();

    sample_comm_vi_disable_fpn_correction(vi_pipe, &g_correction_cfg);

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_void sample_vie_set_ldc_en(ot_vi_pipe vi_pipe, td_bool enable)
{
    td_s32 ret;
    ot_ldc_attr ldc_attr;

    ldc_attr.enable                       = enable;
    ldc_attr.ldc_version                  = OT_LDC_V3;
    ldc_attr.ldc_v3_attr.view_type        = OT_LDC_VIEW_TYPE_ALL;
    ldc_attr.ldc_v3_attr.center_x_offset  = 100; /* 100: center x offset */
    ldc_attr.ldc_v3_attr.center_y_offset  = 100; /* 100: center y offset */
    ldc_attr.ldc_v3_attr.distortion_ratio = 100; /* 100: distortion_ratio */
    ldc_attr.ldc_v3_attr.min_ratio  = 0;

    ret = ss_mpi_vi_set_chn_ldc_attr(vi_pipe, 0, &ldc_attr);
    if (ret != TD_SUCCESS) {
        sample_print("set ldc attr failed.ret:0x%x !\n", ret);
    }
}

static td_void sample_vie_set_rotation_en(ot_vi_pipe vi_pipe, td_bool enable)
{
    td_s32 ret;
    ot_rotation_attr rotation_attr;

    rotation_attr.enable = enable;
    rotation_attr.rotation_type = OT_ROTATION_ANG_FIXED;
    rotation_attr.rotation_fixed = OT_ROTATION_90;

    ret = ss_mpi_vi_set_chn_rotation(vi_pipe, 0, &rotation_attr);
    if (ret != TD_SUCCESS) {
        sample_print("set rotation attr failed.ret:0x%x !\n", ret);
    }
}

static td_void sample_vie_switch_ldc_rotation_en(ot_vi_pipe vi_pipe)
{
    sample_print_pause("press enter key to enable ldc and rotation!\n");

    sample_vie_set_ldc_en(vi_pipe, TD_TRUE);
    sample_vie_set_rotation_en(vi_pipe, TD_TRUE);

    sample_print_pause("press enter key to disable ldc and rotation!\n");

    sample_vie_set_ldc_en(vi_pipe, TD_FALSE);
    sample_vie_set_rotation_en(vi_pipe, TD_FALSE);
}

static td_s32 sample_vie_ldc_rotation(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg;
    const ot_vi_pipe vi_pipe = 0;
    sample_sns_type sns_type = sample_get_sns_type(0);

    g_vb_param.blk_num[0] = 4; /* raw_vb num 4 */
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);
    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_vie_switch_ldc_rotation_en(vi_pipe);
    sample_get_char();

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_void sample_vie_switch_low_delay(ot_vi_pipe vi_pipe, ot_vi_chn vi_chn, ot_vpss_grp vpss_grp,
    ot_vpss_chn vpss_chn)
{
    td_s32 ret;
    ot_low_delay_info low_delay_info;

    low_delay_info.enable = TD_TRUE;
    low_delay_info.line_cnt = 300; /* 300: low delay line cnt */
    low_delay_info.one_buf_en = TD_FALSE;

    sample_print_pause("press enter key to enable pipe low delay!\n");

    ret = ss_mpi_vi_set_pipe_low_delay(vi_pipe, &low_delay_info);
    if (ret != TD_SUCCESS) {
        sample_print("enable pipe low delay failed!\n");
    }
    ret = ss_mpi_vpss_set_chn_low_delay(vpss_grp, vpss_chn, &low_delay_info);
    if (ret != TD_SUCCESS) {
        sample_print("enable vpss low delay failed!\n");
    }

    sample_print_pause("press enter key to disable pipe low delay!\n");

    low_delay_info.enable = TD_FALSE;
    ret = ss_mpi_vi_set_pipe_low_delay(vi_pipe, &low_delay_info);
    if (ret != TD_SUCCESS) {
        sample_print("disable pipe low delay failed!\n");
    }
    ret = ss_mpi_vpss_set_chn_low_delay(vpss_grp, vpss_chn, &low_delay_info);
    if (ret != TD_SUCCESS) {
        sample_print("enable vpss low delay failed!\n");
    }
}

static td_s32 sample_vie_lowdelay(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg = {0};
    sample_sns_type sns_type = sample_get_sns_type(0);
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    const ot_vpss_grp vpss_grp = 0;
    const ot_vpss_chn vpss_chn = 0;

    g_vb_param.blk_num[0] = 4; /* raw_vb num 4 */
    g_vie_sys_cfg.vpss_wrap_en = 0;
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, TD_FALSE);

    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_vie_switch_low_delay(vi_pipe, vi_chn, vpss_grp, vpss_chn);
    sample_get_char();

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_void sample_switch_user_pic(ot_vi_pipe vi_pipe)
{
    td_s32 ret;
    ot_3dnr_attr nr_attr;
    sample_vi_user_pic_type user_pic_type;
    sample_vi_user_frame_info user_frame_info = {0};

    for (user_pic_type = VI_USER_PIC_FRAME; user_pic_type <= VI_USER_PIC_BGCOLOR; user_pic_type++) {
        ret = sample_common_vi_load_user_pic(vi_pipe, user_pic_type, &user_frame_info);
        if (ret != TD_SUCCESS) {
            sample_print("load user pic failed!\n");
            return;
        }

        ret = ss_mpi_vi_set_pipe_user_pic(vi_pipe, &user_frame_info.frame_info);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_vi_set_pipe_user_pic failed!\n");
        }

        ret = ss_mpi_vi_get_pipe_3dnr_attr(vi_pipe, &nr_attr);
        if (ret != TD_SUCCESS) {
            sample_print("vi pipe(%d) get 3dnr_attr failed!\n", vi_pipe);
        }
        nr_attr.enable = TD_FALSE;
        ret = ss_mpi_vi_set_pipe_3dnr_attr(vi_pipe, &nr_attr);
        if (ret != TD_SUCCESS) {
            sample_print("vi pipe(%d) set 3dnr_attr failed!\n", vi_pipe);
        }

        sample_print_pause("press enter key to enable user pic!\n");
        ret = ss_mpi_vi_enable_pipe_user_pic(vi_pipe);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_vi_enable_pipe_user_pic failed!\n");
        }

        sample_print_pause("press enter key to disable user pic!\n");
        ret = ss_mpi_vi_disable_pipe_user_pic(vi_pipe);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_vi_disable_pipe_user_pic failed!\n");
        }

        nr_attr.enable = TD_TRUE;
        ret = ss_mpi_vi_set_pipe_3dnr_attr(vi_pipe, &nr_attr);
        if (ret != TD_SUCCESS) {
            sample_print("vi pipe(%d) set 3dnr_attr failed!\n", vi_pipe);
        }

        sleep(1);
        sample_common_vi_unload_user_pic(&user_frame_info);
    }
}

static td_s32 sample_vie_user_pic(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg = {0};
    sample_sns_type sns_type = sample_get_sns_type(0);
    const ot_vi_pipe vi_pipe = 0;

    g_vb_param.blk_num[0] = 4; /* raw_vb num 4 */
    g_vb_param.blk_num[1] = 4;
    g_vie_sys_cfg.vpss_wrap_en = 0;
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, TD_FALSE);

    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_switch_user_pic(vi_pipe);
    sample_get_char();

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_void sample_vi_get_two_sensor_vi_cfg(sample_sns_type sns_type, sample_vi_cfg vi_cfg[], size_t size)
{
    const ot_vi_pipe vi_pipe = 1; /* dev1 bind pipe1 */
    if (size < 2) { /* need 2 sensor cfg */
        return;
    }
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[1]);
    vi_cfg[0].mipi_info.divide_mode = LANE_DIVIDE_MODE_1;
#ifdef OT_FPGA
    vi_cfg[1].sns_info.bus_id = 0; /* i2c0 */
#else
    vi_cfg[1].sns_info.bus_id = 1; /* i2c1 */
#endif
    vi_cfg[1].sns_info.sns_clk_src = 1;
    vi_cfg[1].sns_info.sns_rst_src = 1;
    sample_comm_vi_get_mipi_info_by_dev_id(sns_type, 0, &vi_cfg[0].mipi_info); /* dev 0 */
    sample_comm_vi_get_mipi_info_by_dev_id(sns_type, 1, &vi_cfg[1].mipi_info); /* dev 1 */
    vi_cfg[1].dev_info.vi_dev = 1;
    vi_cfg[1].bind_pipe.pipe_id[0] = vi_pipe;
    vi_cfg[1].grp_info.grp_num = 1;
    vi_cfg[1].grp_info.fusion_grp[0] = 1;
    vi_cfg[1].grp_info.fusion_grp_attr[0].pipe_id[0] = vi_pipe;

    /* total performance does not support 6M30. */
    /* total performance does not support 6M30. */
    if ((sns_type == HY006_3814_0011_MIPI_4M_30FPS_10BIT) || (sns_type == HY006_3814_0011_MIPI_4M_30FPS_10BIT) ||
        (sns_type == SC4336P_MIPI_4M_30FPS_10BIT) || (sns_type == SC4336P_MIPI_4M_30FPS_10BIT) ||
        (sns_type == SC450AI_MIPI_4M_30FPS_10BIT) || (sns_type == SC500AI_MIPI_5M_30FPS_10BIT)) {
        vi_cfg[0].pipe_info[0].pipe_attr.frame_rate_ctrl.src_frame_rate = 30; /* 30: src_rate */
        vi_cfg[0].pipe_info[0].pipe_attr.frame_rate_ctrl.dst_frame_rate = 20; /* 20: dst_rate */
        vi_cfg[1].pipe_info[0].pipe_attr.frame_rate_ctrl.src_frame_rate = 30; /* 30: src_rate */
        vi_cfg[1].pipe_info[0].pipe_attr.frame_rate_ctrl.dst_frame_rate = 20; /* 20: dst_rate */
    }
}

/* Set pin_mux i2c0 & i2c1 & sensor0 & sensor1 & MIPI0 & MIPI1 before using this sample! */
static td_s32 sample_vie_two_sensor(td_void)
{
    sample_vi_cfg vi_cfg[2];
    sample_vpss_cfg vpss_cfg;
    sample_sns_type sns_type = sample_get_sns_type(0);

    g_vie_sys_cfg.route_num = 2; /* 2: route_num */

    g_vb_param.blk_num[0] = 0; /* raw_vb num 5 */
    g_vb_param.blk_num[1] = 6; /* yuv_vb num 6 */
    g_vb_param.blk_num[2] = 6; /* vpss chn 2, yuv_vb num 6 */

    sample_vi_get_two_sensor_vi_cfg(sns_type, vi_cfg, sizeof(vi_cfg) / sizeof(vi_cfg[0]));
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    sample_vie_get_pipe_wrap_line(vi_cfg, g_vie_sys_cfg.route_num);
    sample_vie_vpss_get_wrap_cfg(&g_vie_sys_cfg, &vpss_cfg);
    if (sample_vie_start_route(vi_cfg, &vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_get_char();

    sample_vie_stop_route(vi_cfg, g_vie_sys_cfg.route_num);
    return TD_SUCCESS;
}

static td_void sample_vie_usage(const char *prg_name)
{
    printf("usage : %s <index> <rtsp_enable> [sns_type]\n", prg_name);
    printf("index:\n");
    printf("    (0) all mode route          :vi linear(online/offline) -> vpss(online/offline) -> venc.\n");
    printf("    (1) wdr route               :vi wdr(online/offline) -> vpss(online/offline) -> venc.\n");
    printf("    (2) fpn calibrate & correct :vi fpn calibrate & correct(offline) -> vpss(offline) -> venc.\n");
    printf("    (3) ldc rotation            :vi ldc/rotation(offline) -> vpss(offline) -> venc.\n");
    printf("    (4) low delay               :vi(pipe lowdelay) -> vpss(lowdelay) -> venc.\n");
    printf("    (5) user pic                :vi user pic(offline) -> vpss(offline) -> venc.\n");
    printf("    (6) two sensor              :vi two sensor(offline) -> vpss(offline) -> venc.\n");
    printf("    (7) isp run switch mode     :vi linear switch to wdr(online/offline) -> vpss(online/offline) -> venc.\n");
    printf("    (8) switch resolution       :vi 4M switch to 1080P(online/offline) -> vpss(online/offline) -> venc.\n");
    printf("    (9) isp run be switch mode  :vi linear switch to wdr(offline) -> vpss(offline) -> venc.\n");
    printf("rtsp_enable:\n");
    printf("    (0) disable rtsp : venc encode to local file (recording).\n");
    printf("    (1) enable rtsp  : venc encode to rtsp stream (no local recording).\n");
    printf("sns_type: (optional)\n");
    printf("    NOTE: If sns_type is specified, sensor0~sensor1 will ALL use this type.\n");
    printf("          If not specified, sensor0~sensor1 use default values from Makefile.\n");
    printf("    (4)  SC4336P_MIPI_4M_30FPS_10BIT\n");
    printf("    (5)  SC4336P_MIPI_3M_30FPS_10BIT\n");
    printf("    (6)  SC4336P_MIPI_2M_30FPS_10BIT\n");
    printf("    (7)  SC450AI_MIPI_4M_30FPS_10BIT\n");
    printf("    (9)  SC500AI_MIPI_5M_30FPS_10BIT\n");

}

static td_void sample_vie_handle_sig(td_s32 signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        g_sig_flag = 1;
    }
}

static td_void sample_register_sig_handler(td_void (*sig_handle)(td_s32))
{
    struct sigaction sa;

    (td_void)memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handle;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, TD_NULL);
    sigaction(SIGTERM, &sa, TD_NULL);
}

static td_s32 sample_vie_execute_case(td_u32 case_index)
{
    td_s32 ret;

    switch (case_index) {
        case 0: /* 0 all mode route */
            ret = sample_vie_all_mode();
            break;
        case 1: /* 1 wdr route */
            ret = sample_vie_wdr();
            break;
        case 2: /* 2 fpn calibrate and correct */
            ret = sample_vie_fpn();
            break;
        case 3: /* 3 ldc and rotation */
            ret = sample_vie_ldc_rotation();
            break;
        case 4: /* 4 low delay */
            ret = sample_vie_lowdelay();
            break;
        case 5: /* 5 user pic */
            ret = sample_vie_user_pic();
            break;
        case 6: /* 6 two sensor */
            ret = sample_vie_two_sensor();
            break;
        case 7: /* 7 switch mode */
            ret = sample_vie_switch_mode();
            break;
        case 8: /* 8 switch resolution */
            ret = sample_vie_switch_resolution();
            break;
        case 9: /* 9 run be switch mode */
            ret = sample_vie_run_be_switch_mode();
            break;
        default:
            ret = TD_FAILURE;
            break;
    }

    return ret;
}

static td_s32 sample_vie_msg_proc_vb_pool_share(td_s32 pid)
{
    td_s32 ret;
    td_u32 i;
    td_bool isp_states[OT_VI_MAX_PIPE_NUM];
#ifndef SAMPLE_MEM_SHARE_ENABLE
    ot_vb_common_pools_id pools_id = {0};

    if (ss_mpi_vb_get_common_pool_id(&pools_id) != TD_SUCCESS) {
        sample_print("get common pool_id failed!\n");
        return TD_FAILURE;
    }

    for (i = 0; i < pools_id.pool_cnt; ++i) {
        if (ss_mpi_vb_pool_share(pools_id.pool[i], pid) != TD_SUCCESS) {
            sample_print("vb pool share failed!\n");
            return TD_FAILURE;
        }
    }
#endif
    ret = sample_comm_vi_get_isp_run_state(isp_states, OT_VI_MAX_PIPE_NUM);
    if (ret != TD_SUCCESS) {
        sample_print("get isp states fail\n");
        return TD_FAILURE;
    }

    for (i = 0; i < OT_VI_MAX_PIPE_NUM; i++) {
        if (!isp_states[i]) {
            continue;
        }
        ret = ss_mpi_isp_mem_share(i, pid);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_isp_mem_share vi_pipe %u, pid %d fail\n", i, pid);
        }
    }

    return TD_SUCCESS;
}

static td_void sample_vie_msg_proc_vb_pool_unshare(td_s32 pid)
{
    td_s32 ret;
    td_u32 i;
    td_bool isp_states[OT_VI_MAX_PIPE_NUM];
#ifndef SAMPLE_MEM_SHARE_ENABLE
    ot_vb_common_pools_id pools_id = {0};
    if (ss_mpi_vb_get_common_pool_id(&pools_id) == TD_SUCCESS) {
        for (i = 0; i < pools_id.pool_cnt; ++i) {
            ret = ss_mpi_vb_pool_unshare(pools_id.pool[i], pid);
            if (ret != TD_SUCCESS) {
                sample_print("ss_mpi_vb_pool_unshare vi_pipe %u, pid %d fail\n", pools_id.pool[i], pid);
            }
        }
    }
#endif
    ret = sample_comm_vi_get_isp_run_state(isp_states, OT_VI_MAX_PIPE_NUM);
    if (ret != TD_SUCCESS) {
        sample_print("get isp states fail\n");
        return;
    }

    for (i = 0; i < OT_VI_MAX_PIPE_NUM; i++) {
        if (!isp_states[i]) {
            continue;
        }
        ret = ss_mpi_isp_mem_unshare(i, pid);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_isp_mem_unshare vi_pipe %u, pid %d fail\n", i, pid);
        }
    }
}

static td_s32 sample_vie_ipc_msg_proc(const sample_ipc_msg_req_buf *msg_req_buf,
    td_bool *is_need_fb, sample_ipc_msg_res_buf *msg_res_buf)
{
    td_s32 ret;

    if (msg_req_buf == TD_NULL || is_need_fb == TD_NULL) {
        return TD_FAILURE;
    }

    /* need feedback default */
    *is_need_fb = TD_TRUE;

    switch ((sample_msg_type)msg_req_buf->msg_type) {
        case SAMPLE_MSG_TYPE_VB_POOL_SHARE_REQ: {
            if (msg_res_buf == TD_NULL) {
                return TD_FAILURE;
            }
            ret = sample_vie_msg_proc_vb_pool_share(msg_req_buf->msg_data.pid);
            msg_res_buf->msg_type = SAMPLE_MSG_TYPE_VB_POOL_SHARE_RES;
            msg_res_buf->msg_data.is_req_success = (ret == TD_SUCCESS) ? TD_TRUE : TD_FALSE;
            break;
        }
        case SAMPLE_MSG_TYPE_VB_POOL_UNSHARE_REQ: {
            if (msg_res_buf == TD_NULL) {
                return TD_FAILURE;
            }
            sample_vie_msg_proc_vb_pool_unshare(msg_req_buf->msg_data.pid);
            msg_res_buf->msg_type = SAMPLE_MSG_TYPE_VB_POOL_UNSHARE_RES;
            msg_res_buf->msg_data.is_req_success = TD_TRUE;
            break;
        }
        default: {
            printf("unsupported msg type(%ld)!\n", msg_req_buf->msg_type);
            return TD_FAILURE;
        }
    }
    return TD_SUCCESS;
}

#ifdef __LITEOS__
td_s32 app_main(td_s32 argc, td_char *argv[])
#else
td_s32 main(td_s32 argc, td_char *argv[])
#endif
{
    td_s32 ret;
    td_u32 index;
    td_s32 push_rtsp = 0;
    td_char *end_ptr = TD_NULL;

    if (argc < 0x3 || argc > 0x4) { /* 2:arg num */
        sample_vie_usage(argv[0]);
        return TD_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2)) { /* 2:arg num */
        sample_vie_usage(argv[0]);
        return TD_FAILURE;
    }

    if (strlen(argv[1]) >= 2 || strlen(argv[1]) == 0 || !check_digit(argv[1][0])) { /* 2:arg len */
        sample_vie_usage(argv[0]);
        return TD_FAILURE;
    }

    index = (td_u32)strtol(argv[1], &end_ptr, 10); /* base 10, argv[1] has been check between [0, 10] */
    if ((end_ptr == argv[1]) || (*end_ptr) != '\0') {
        sample_vie_usage(argv[0]);
        return TD_FAILURE;
    }

#ifndef __LITEOS__
    sample_register_sig_handler(sample_vie_handle_sig);
#endif

    if (sample_ipc_server_init(sample_vie_ipc_msg_proc) != TD_SUCCESS) {
        printf("sample_ipc_server_init failed!!!\n");
    }

    push_rtsp = (td_u32)strtol(argv[2], &end_ptr, 10);
    if ((end_ptr == argv[2]) || (*end_ptr) != '\0') {
        sample_vie_usage(argv[0]);
        return TD_FAILURE;
    }
    sample_comm_venc_set_push_rtsp((push_rtsp == 1) ? TD_TRUE : TD_FALSE);
    if (push_rtsp){
        const char *rtsp_ip = "0.0.0.0";
        rtsp_server_start(rtsp_ip, 554);
        rtsp_session_create(0, 1);  //ven0 h265
        rtsp_session_create(1, 1);  //ven1 h265

        if (index == 6) { // two sensor case, create two more session for second sensor
            rtsp_session_create(2, 1);  //ven2 h265
            rtsp_session_create(3, 1);  //ven3 h265
        }
    }

    if (argc == 4) {
        sample_sns_type sns_type;
        sns_type = (sample_sns_type)strtol(argv[3], &end_ptr, 10);
        if ((end_ptr == argv[3]) || (*end_ptr) != '\0') {
            sample_vie_usage(argv[0]);
            return TD_FAILURE;
        }
        sample_set_all_sns_type(sns_type);
    }

    ret = sample_vie_execute_case(index);
    if ((ret == TD_SUCCESS) && (g_sig_flag == 0)) {
        printf("\033[0;32mprogram exit normally!\033[0;39m\n");
    } else {
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    if (push_rtsp){
        rtsp_server_stop();
    }

    sample_ipc_server_deinit();
    return ret;
}
