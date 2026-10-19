/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "ot_common_isp.h"
#include "ss_mpi_ae.h"
#include "ot_isp_debug.h"
#include "sample_comm.h"
#include "ss_mpi_isp.h"

static volatile sig_atomic_t g_sig_flag = 0;
#define VB_BLK_CNT 10
#define USE_FRAME_NUM 3
#define DET50_THRESHOLD 3
#define DET60_THRESHOLD 3
#define DET_TIME_OUT 5
#define DET_UNKNOW_THRESHOLD 4
#define DET_NONE_THRESHOLD 3
#define SLEEP_TIME 500000
#define FREQ50 50
#define FREQ60 60
#define FPS30 30.0
#define FPS25 25.0
#define ISO220 220
#define EXP4400 4400
#define EXP5500 5500
#define VPSS_CHN 2
#define DET_IMG_WIDTH 1920
#define DET_IMG_HEIGHT 1080

static volatile sig_atomic_t g_flicker_sig_flag = 0;

typedef struct {
    sample_vi_cfg vi_config;
    ot_venc_chn venc_chn;
    sample_comm_venc_chn_param venc_param;
    ot_vi_pipe vi_pipe;
    ot_vi_chn vi_chn;
    ot_vpss_grp vpss_grp;
    ot_vpss_chn vpss_chn[VPSS_CHN];
    ot_vpss_grp_attr vpss_grp_attr;
    sample_vpss_chn_attr vpss_chn_attr;
}calcflicker_prev;

typedef struct flicker_cnt {
    td_u8 det_50;
    td_u8 det_60;
    td_u8 det_unknow;
    td_u8 det_none;
    td_u8 try_50;
    td_u8 try_60;
} flicker_cnt;

static td_void flicker_getchar()
{
    printf("--------------- press enter key to get start! ---------------\n");
    if (g_flicker_sig_flag == 1) {
        return;
    }

    (td_void)getchar();

    if (g_flicker_sig_flag == 1) {
        return;
    }
}

td_s32 sample_calcflicker_start_vi(sample_vi_cfg *vi_cfg)
{
    td_s32  ret;

    ret = sample_comm_vi_start_vi(vi_cfg);
    if (TD_SUCCESS != ret) {
        sample_print("SAMPLE_Calcflicker start vi failed!\n");
        return ret;
    }

    return ret;
}

static td_s32 sample_calcflicker_stop_vi(sample_vi_cfg *vi_cfg)
{
    sample_comm_vi_stop_vi(vi_cfg);
    return TD_SUCCESS;
}

static td_void sample_vi_get_default_vb_config(ot_size *size, ot_vb_cfg *vb_cfg)
{
    ot_vb_calc_cfg calc_cfg;
    ot_pic_buf_attr samlple_calflicker_buf_attr;

    (td_void)memset_s(vb_cfg, sizeof(ot_vb_cfg), 0, sizeof(ot_vb_cfg));
    vb_cfg->max_pool_cnt = 128; /* 128 pool limit */
    samlple_calflicker_buf_attr.align         = OT_DEFAULT_ALIGN;
    samlple_calflicker_buf_attr.bit_width     = OT_DATA_BIT_WIDTH_8;
    samlple_calflicker_buf_attr.width         = size->width;
    samlple_calflicker_buf_attr.height        = size->height;
    samlple_calflicker_buf_attr.compress_mode = OT_COMPRESS_MODE_NONE;
    samlple_calflicker_buf_attr.video_format = OT_VIDEO_FORMAT_LINEAR;
    samlple_calflicker_buf_attr.pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    ot_common_get_pic_buf_cfg(&samlple_calflicker_buf_attr, &calc_cfg);

    vb_cfg->common_pool[0].blk_size = calc_cfg.vb_size;
    vb_cfg->common_pool[0].blk_cnt  = VB_BLK_CNT;
}

static td_s32 sample_calcflicker_sys_init(ot_vi_vpss_mode_type mode_type, ot_vi_aiisp_mode aiisp_mode)
{
    td_s32 ret;
    ot_size size;
    ot_vb_cfg vb_cfg;
    td_u32 supplement_config;

    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &size);
    sample_vi_get_default_vb_config(&size, &vb_cfg);

    supplement_config = OT_VB_SUPPLEMENT_BNR_MOT_MASK;
    ret = sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_config);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    ret = sample_comm_vi_set_vi_vpss_mode(mode_type, aiisp_mode);
    if (ret != TD_SUCCESS) {
        sample_comm_sys_exit();

        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_s32 sample_calcflicker_start_vpss(calcflicker_prev *calcflicker_prev, ot_size *in_size)
{
    ot_vpss_grp_attr *grp_attr = &calcflicker_prev->vpss_grp_attr;
    sample_vpss_chn_attr *vpss_chn_attr = &calcflicker_prev->vpss_chn_attr;
    ot_vpss_chn_attr *attr = TD_NULL;
    td_bool vpss_chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_TRUE, TD_FALSE};

    sample_comm_vpss_get_default_grp_attr(grp_attr);
    grp_attr->max_width  = in_size->width;
    grp_attr->max_height = in_size->height;
    attr = &vpss_chn_attr->chn_attr[0];
    sample_comm_vpss_get_default_chn_attr(attr);
    vpss_chn_attr->chn_attr[0].width  = in_size->width;
    vpss_chn_attr->chn_attr[0].height = in_size->height;
    attr = &vpss_chn_attr->chn_attr[1];
    sample_comm_vpss_get_default_chn_attr(attr);
    vpss_chn_attr->chn_attr[1].width  = DET_IMG_WIDTH;
    vpss_chn_attr->chn_attr[1].height = DET_IMG_HEIGHT;
    vpss_chn_attr->chn_attr[1].compress_mode = OT_COMPRESS_MODE_NONE;
    vpss_chn_attr->chn_attr[1].depth = 2; /* channel copy 2 */
    memcpy_s(vpss_chn_attr->chn_enable, sizeof(vpss_chn_attr->chn_enable),
             vpss_chn_enable, sizeof(vpss_chn_attr->chn_enable));
    vpss_chn_attr->chn_array_size = OT_VPSS_MAX_PHYS_CHN_NUM;
    return sample_common_vpss_start(calcflicker_prev->vpss_grp,
                                    &calcflicker_prev->vpss_grp_attr,
                                    &calcflicker_prev->vpss_chn_attr);
}
static td_void sample_calcflicker_get_default_venc_param(sample_sns_type sns_type,
    sample_comm_venc_chn_param *venc_param)
{
    ot_size size;

    sample_comm_vi_get_size_by_sns_type(sns_type, &size);

    venc_param->frame_rate = 30;    // 30: frame rate
    venc_param->stats_time = 2;     // 2: default stats_time
    venc_param->gop = 60;           // 60: default gop
    venc_param->venc_size.width = size.width;
    venc_param->venc_size.height = size.height;
    venc_param->size = PIC_1080P;
    venc_param->profile = 0;
    venc_param->is_rcn_ref_share_buf = TD_FALSE;
    venc_param->gop_attr.gop_mode = OT_VENC_GOP_MODE_NORMAL_P;
    venc_param->gop_attr.normal_p.ip_qp_delta = 2;  // 2: default delta
    venc_param->type = OT_PT_H265;
    venc_param->rc_mode = SAMPLE_RC_CBR;
}

static td_void sample_calcflicker_flicker_init(calcflicker_prev *flicker)
{
    flicker->vi_pipe              = 0;
    flicker->vi_chn               = 0;
    flicker->vpss_grp             = 0;
    flicker->vpss_chn[0]          = 0;
    flicker->vpss_chn[1]          = 1;
    flicker->venc_chn             = 0;
}

td_s32 sample_calcflicker_start_prev(calcflicker_prev *flicker)
{
    sample_sns_type  sns_type = SENSOR0_TYPE;
    ot_size             in_size;
    sample_calcflicker_flicker_init(flicker);
    if (sample_calcflicker_sys_init(OT_VI_OFFLINE_VPSS_OFFLINE, OT_VI_AIISP_MODE_DEFAULT) != TD_SUCCESS) {
        goto sys_init_failed;
    }
    sample_comm_vi_get_size_by_sns_type(sns_type, &in_size);
    sample_comm_vi_get_default_vi_cfg(sns_type, &flicker->vi_config);
    sample_calcflicker_get_default_venc_param(sns_type, &flicker->venc_param);
    if (sample_calcflicker_start_vi(&flicker->vi_config) != TD_SUCCESS) {
        sample_print("sample_calcflicker_start_vi failed\n");
        goto EXIT;
    }
    if (sample_calcflicker_start_vpss(flicker, &in_size) != TD_SUCCESS) {
        sample_print("sample_calcflicker_start_vpss failed\n");
        goto EXIT1;
    }
    if (sample_comm_venc_start(flicker->venc_chn, &flicker->venc_param) != TD_SUCCESS) {
        sample_print("sample_comm_venc_start failed\n");
        goto EXIT2;
    }
    if (sample_comm_vi_bind_vpss(flicker->vi_pipe, flicker->vi_chn,
                                 flicker->vpss_grp, flicker->vpss_chn[0]) != TD_SUCCESS) {
        sample_print("sample_comm_vi_bind_vpss failed\n");
        goto EXIT3;
    }
    if (sample_comm_vi_bind_venc(flicker->vi_pipe, flicker->vi_chn, flicker->venc_chn) != TD_SUCCESS) {
        sample_print("sample_comm_vpss_bind_venc failed!\n");
        goto EXIT4;
    }
    if ((sample_comm_venc_start_get_stream(&flicker->venc_chn, 1)) != TD_SUCCESS) {    // 1: chn num
        sample_print("sample_comm_venc_start_get_stream failed!\n");
        goto EXIT5;
    }
    return TD_SUCCESS;

EXIT5:
    sample_comm_vi_un_bind_venc(flicker->vi_pipe, flicker->vi_chn, flicker->venc_chn);

EXIT4:
    sample_comm_vi_un_bind_vpss(flicker->vi_pipe, flicker->vi_chn, flicker->vpss_grp, flicker->vpss_chn[0]);

EXIT3:
    sample_comm_venc_stop(flicker->venc_chn);

EXIT2:
    sample_common_vpss_stop(flicker->vpss_grp, flicker->vpss_chn_attr.chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
EXIT1:
    sample_calcflicker_stop_vi(&flicker->vi_config);
EXIT:
    sample_comm_sys_exit();
sys_init_failed:
    return TD_FAILURE;
}

td_s32 sample_calcflicker_stop_prev(calcflicker_prev *calcflicker_prev)
{
    td_s32 ret;
    if (calcflicker_prev == TD_NULL) {
        sample_print("err: calcflicker_prev is NULL \n");
        return TD_FAILURE;
    }
    ret = sample_comm_venc_stop_get_stream(1);    // 1: chn num
    if (ret != TD_SUCCESS) {
        return ret;
    }

    ret = sample_comm_venc_stop(calcflicker_prev->venc_chn);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    ret = sample_comm_vi_un_bind_venc(calcflicker_prev->vi_pipe, calcflicker_prev->vi_chn, calcflicker_prev->venc_chn);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    ret += sample_comm_vi_un_bind_vpss(calcflicker_prev->vi_pipe,
                                       calcflicker_prev->vi_chn,
                                       calcflicker_prev->vpss_grp,
                                       calcflicker_prev->vpss_chn[0]);
    ret += sample_common_vpss_stop(calcflicker_prev->vpss_grp,
                                   calcflicker_prev->vpss_chn_attr.chn_enable,
                                   OT_VPSS_MAX_PHYS_CHN_NUM);
    ret += sample_calcflicker_stop_vi(&calcflicker_prev->vi_config);

    sample_comm_sys_exit();
    return ret;
}

/******************************************************************************
* function : show usage
******************************************************************************/
void sample_calc_flicker_usage(const char *s_prg_nm)
{
    printf("usage : %s <index>\n", s_prg_nm);
    printf("index:\n");

    printf("\t 0)auto calculate flicker and anti_flicker.\n");
    printf("\t 1)only calculate flicker type for 1 times.\n");
    printf("\t 2)only calculate flicker type for 3 times.\n");

    printf("\t calc flicker normal -mode under wall mount.\n");
    printf("\t if you have any questions, please look at readme.txt!\n");
    return;
}

td_s32 sample_calc_flicker_type(const calcflicker_prev *calcflicker_prev,
                                ot_isp_calc_flicker_output *output_param)
{
    td_u16 i = 0;
    td_u16 frame_num = USE_FRAME_NUM;
    ot_vi_pipe_attr vi_chn_attr = {0};
    ot_vpss_chn vpss_chn = 1;
    ot_vpss_chn_attr chn_attr;
    ot_vpss_grp_attr grp_attr;

    td_s32 milli_sec = -1;
    td_s32 ret = TD_SUCCESS;
    ot_video_frame_info frame[USE_FRAME_NUM] = {0};
    ot_isp_calc_flicker_input input_line;
    ot_vpss_grp grp = calcflicker_prev->vpss_grp;
    ot_isp_pub_attr pub_attr;
    ret = ss_mpi_vpss_get_chn_attr(grp, calcflicker_prev->vpss_chn[1], &chn_attr);
    ret += ss_mpi_vpss_get_grp_attr(grp, &grp_attr);
    ret += ss_mpi_vi_get_pipe_attr(calcflicker_prev->vi_pipe,  &vi_chn_attr);
    ret += ss_mpi_isp_get_pub_attr(calcflicker_prev->vi_pipe, &pub_attr);
    if (ret != TD_SUCCESS) {
        printf("get attr failed !!! in %s!\n", __FUNCTION__);
        return ret;
    }

    /* get frame buffer */
    for (i = 0; i < frame_num; i++) {
        ss_mpi_vpss_get_chn_frame(grp, vpss_chn, &frame[i], milli_sec);
    }
    input_line.lines_per_second = chn_attr.height * (td_u32)pub_attr.frame_rate;
    ret = ss_mpi_isp_calc_flicker_type(0, &input_line, output_param, frame, frame_num);
    if (ret != TD_SUCCESS) {
        printf("the state of calc type function error !!! in %s!\n", __FUNCTION__);
    }
    for (i = 0; i < frame_num; i++) {
        ss_mpi_vpss_release_chn_frame(grp, vpss_chn, &frame[i]);
    }
    return ret;
}

td_void sample_calc_flicker_mpi_init(ot_vi_pipe vi_pipe,
                                     ot_isp_exposure_attr *exp_attr,
                                     ot_isp_exposure_attr *exp_attr_dft,
                                     ot_isp_exp_info *exp_info)
{
    ss_mpi_isp_get_exposure_attr(vi_pipe, exp_attr);
    ss_mpi_isp_get_exposure_attr(vi_pipe, exp_attr_dft);
    ss_mpi_isp_query_exposure_info(vi_pipe, exp_info);
}

td_s32 sample_flicker_50hz_process(ot_vi_pipe vi_pipe, ot_isp_exp_info *exp_info, ot_isp_pub_attr *pub_attr,
                                   ot_isp_exposure_attr *exp_attr, flicker_cnt *cnt)
{
    td_s32 ret = 0;
    cnt->det_50++;
    cnt->det_60 = 0;
    cnt->det_none = 0;
    cnt->det_unknow = 0;
    if (cnt->det_50 >= DET50_THRESHOLD) {
        if ((exp_info->exp_time < EXP5500) && (exp_info->iso < ISO220)) {
            pub_attr->frame_rate = FPS25;
            ret = ss_mpi_isp_set_pub_attr(vi_pipe, pub_attr);
            printf("----- set pub frame rate to 25 -----\n");
        } else {
            exp_attr->auto_attr.antiflicker.enable = TD_TRUE;
            exp_attr->auto_attr.antiflicker.frequency = FREQ50;
            ret = ss_mpi_isp_set_exposure_attr(vi_pipe, exp_attr);
            printf("----- set antiflicker frequency 50 hz -----\n");
        }
        usleep(SLEEP_TIME);
    }
    return ret;
}

td_s32 sample_flicker_60hz_process(ot_vi_pipe vi_pipe, ot_isp_exp_info *exp_info, ot_isp_pub_attr *pub_attr,
                                   ot_isp_exposure_attr *exp_attr, flicker_cnt *cnt)
{
    td_s32 ret = 0;
    cnt->det_50 = 0;
    cnt->det_60++;
    cnt->det_none = 0;
    cnt->det_unknow = 0;
    if (cnt->det_60 >= DET60_THRESHOLD) {
        if ((exp_info->exp_time < EXP4400) && (exp_info->iso < ISO220)) {
            pub_attr->frame_rate = FPS30;
            ret = ss_mpi_isp_set_pub_attr(vi_pipe, pub_attr);
            printf("----- set pub frame rate to 30 -----\n");
        } else {
            exp_attr->auto_attr.antiflicker.enable = TD_TRUE;
            exp_attr->auto_attr.antiflicker.frequency = FREQ60;
            ret = ss_mpi_isp_set_exposure_attr(vi_pipe, exp_attr);
            printf("----- set antiflicker frequency 60 hz -----\n");
        }
        usleep(SLEEP_TIME);
    }
    return ret;
}

td_s32 sample_flicker_set_antiflicker(ot_vi_pipe vi_pipe, ot_isp_exposure_attr *exp_attr, td_u8 freq)
{
    td_s32 ret;
    printf("---- try to use %u_hz ----\n", freq);
    exp_attr->auto_attr.antiflicker.enable = TD_TRUE;
    exp_attr->auto_attr.antiflicker.frequency = freq;
    ret = ss_mpi_isp_set_exposure_attr(vi_pipe, exp_attr);
    usleep(SLEEP_TIME);
    return ret;
}

td_s32 sample_flicker_unknow_process(ot_vi_pipe vi_pipe, ot_isp_exposure_attr *exp_attr, flicker_cnt *cnt)
{
    td_s32 ret = 0;
    cnt->det_50 = 0;
    cnt->det_60 = 0;
    cnt->det_none = 0;
    cnt->det_unknow++;
    if (cnt->det_unknow >= DET_UNKNOW_THRESHOLD) {
        if (cnt->try_50 == 0) {
            ret = sample_flicker_set_antiflicker(vi_pipe, exp_attr, FREQ50);
            cnt->det_unknow = 0;
            cnt->try_50 = 1;
        } else if (cnt->try_60 == 0) {
            ret = sample_flicker_set_antiflicker(vi_pipe, exp_attr, FREQ60);
            cnt->det_unknow = 0;
            cnt->try_60 = 1;
        }
    }
    return ret;
}
td_void sample_flicker_none_process(flicker_cnt *cnt)
{
    cnt->det_50 = 0;
    cnt->det_60 = 0;
    cnt->det_unknow = 0;
    cnt->det_none++;
}

td_s32 sample_auto_calc_flicker_and_anti_flicker(const calcflicker_prev *calcflicker_prev)
{
    td_u8 detect_time_out = FREQ50;
    flicker_cnt cnt = {0};
    ot_vi_pipe vi_pipe = 0;
    td_s32 ret = TD_FAILURE;
    ot_isp_exposure_attr exp_attr, exp_attr_dft;
    ot_isp_pub_attr pub_attr;
    ot_isp_exp_info exp_info;
    ot_isp_calc_flicker_output output_param = { 0 };
    sample_calc_flicker_mpi_init(vi_pipe, &exp_attr, &exp_attr_dft, &exp_info);
    while (detect_time_out--) {
        ret = sample_calc_flicker_type(calcflicker_prev, &output_param);
        ss_mpi_isp_query_exposure_info(vi_pipe, &exp_info);
        ss_mpi_isp_get_pub_attr(vi_pipe, &pub_attr);
        if (OT_ISP_FLICKER_TYPE_50HZ == output_param.flicker_type) {
            printf("##### FLICKER_TYPE_50HZ %u #####\n", detect_time_out);
            sample_flicker_50hz_process(vi_pipe, &exp_info, &pub_attr, &exp_attr, &cnt);
            if (cnt.det_50 > DET_TIME_OUT) {
                ret = ss_mpi_isp_set_exposure_attr(vi_pipe, &exp_attr_dft);
                break;
            }
        } else if (OT_ISP_FLICKER_TYPE_60HZ == output_param.flicker_type) {
            printf("##### FLICKER_TYPE_60HZ %u #####\n", detect_time_out);
            sample_flicker_60hz_process(vi_pipe, &exp_info, &pub_attr, &exp_attr, &cnt);
            if (cnt.det_60 > DET_TIME_OUT) {
                ret = ss_mpi_isp_set_exposure_attr(vi_pipe, &exp_attr_dft);
                break;
            }
        } else if (OT_ISP_FLICKER_TYPE_UNKNOW == output_param.flicker_type) {
            printf("##### FLICKER_TYPE_UNKNOW %u #####\n", detect_time_out);
            sample_flicker_unknow_process(vi_pipe, &exp_attr, &cnt);
            if (cnt.det_unknow >= DET_UNKNOW_THRESHOLD && cnt.try_50 == 1 && cnt.try_60 == 1) {
                ret = ss_mpi_isp_set_exposure_attr(vi_pipe, &exp_attr_dft);
                break;
            }
        } else if (OT_ISP_FLICKER_TYPE_NONE == output_param.flicker_type) {
            printf("##### FLICKER_TYPE_NONE %u #####\n", detect_time_out);
            sample_flicker_none_process(&cnt);
            if (cnt.det_none >= DET_UNKNOW_THRESHOLD) {
                printf("no flicker detect more than 5 time\n");
                break;
            }
        }
    }
    return ret;
}

td_s32 sample_calc_flicker_normal(const calcflicker_prev *calcflicker_prev)
{
    td_s32 ret;
    ret = sample_auto_calc_flicker_and_anti_flicker(calcflicker_prev);
    return ret;
}

td_s32 sample_calc_flicker_3_times(const calcflicker_prev *calcflicker_prev)
{
    td_s32 ret;
    ot_vi_pipe vi_pipe = 0;
    ot_isp_exp_info exp_info;
    td_u16 i = 0;
    ot_isp_calc_flicker_output output_param = { 0 };
    ss_mpi_isp_query_exposure_info(vi_pipe, &exp_info);

    for (i = 0; i < 3; i++) { /* test 3 time */
        ret = sample_calc_flicker_type(calcflicker_prev, &output_param);
        if (OT_ISP_FLICKER_TYPE_50HZ == output_param.flicker_type) {
            printf("time: %u, ##### FLICKER_TYPE_50HZ #####\n", i + 1);
        } else if (OT_ISP_FLICKER_TYPE_60HZ == output_param.flicker_type) {
            printf("time: %u, ##### FLICKER_TYPE_60HZ #####\n", i + 1);
        } else if (OT_ISP_FLICKER_TYPE_NONE == output_param.flicker_type) {
            printf("time: %u, ##### FLICKER_TYPE_NONE #####\n", i + 1);
        } else if (OT_ISP_FLICKER_TYPE_UNKNOW == output_param.flicker_type) {
            printf("time: %u, ##### FLICKER_TYPE_UNKNOW #####\n", i + 1);
        }
    }
    return ret;
}

td_s32 sample_calc_flicker_1_times(calcflicker_prev *calcflicker_prev)
{
    td_s32 ret;
    ot_vi_pipe vi_pipe = 0;
    ot_isp_exp_info exp_info;
    ot_isp_calc_flicker_output output_param = { 0 };
    ss_mpi_isp_query_exposure_info(vi_pipe, &exp_info);
    ret = sample_calc_flicker_type(calcflicker_prev, &output_param);
    if (OT_ISP_FLICKER_TYPE_50HZ == output_param.flicker_type) {
        printf("\n##### FLICKER_TYPE_50HZ #####\n");
    } else if (OT_ISP_FLICKER_TYPE_60HZ == output_param.flicker_type) {
        printf("\n##### FLICKER_TYPE_60HZ #####\n");
    } else if (OT_ISP_FLICKER_TYPE_NONE == output_param.flicker_type) {
        printf("\n##### FLICKER_TYPE_NONE #####\n");
    } else if (OT_ISP_FLICKER_TYPE_UNKNOW == output_param.flicker_type) {
        printf("\n##### FLICKER_TYPE_UNKNOW #####\n");
    }
    return ret;
}

static td_void sample_calcflicker_handle_sig(td_s32 signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        g_flicker_sig_flag = 1;
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

/******************************************************************************
* function    : main()
* description : video fisheye preview sample
******************************************************************************/
#ifdef __LITEOS__
td_s32 app_main(td_s32 argc, td_char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    td_s32             ret        = TD_FAILURE;
    if (argc < 2 || argc > 2) {  /* 2 get sample 2 */
        sample_calc_flicker_usage(argv[0]);
        return TD_FAILURE;
    }
    if (!strncmp(argv[1], "-h", 2)) {  /* 2 get sample */
        sample_calc_flicker_usage(argv[0]);
        return TD_SUCCESS;
    }
    if (strlen(argv[1]) != 1  || !check_digit(argv[1][0]) || argv[1][0] > '2') { // 2 max idx
        sample_calc_flicker_usage(argv[0]);
        return TD_FAILURE;
    }

#ifndef __LITEOS__
    sample_register_sig_handler(sample_calcflicker_handle_sig);
#endif
    calcflicker_prev calcflicker_prev;
    ret = sample_calcflicker_start_prev(&calcflicker_prev);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    (td_void)flicker_getchar();
    if (g_flicker_sig_flag == 0) {
        printf("\033[0;32mprogram cannot be terminated using Ctrl + C!\033[0;39m\n");
        switch (*argv[1]) {
                /* VI/VPSS - VO. embeded isp, phychn channel preview. */
            case '0':
                ret = sample_calc_flicker_normal(&calcflicker_prev);
                break;
            case '1':
                ret = sample_calc_flicker_1_times(&calcflicker_prev);
                break;
            case '2':
                ret = sample_calc_flicker_3_times(&calcflicker_prev);
                break;
            default:
                ret = TD_FAILURE;
                sample_print("the index is invalid!\n");
                sample_calc_flicker_usage(argv[0]);
                break;
        }
        if (ret == TD_FAILURE) {
            goto EXIT;
        }
        if (ret == TD_SUCCESS) {
            printf("\033[0;32mprogram exit normally!\033[0;39m\n");
        }
        sample_pause();
    }
EXIT:
    ret = sample_calcflicker_stop_prev(&calcflicker_prev);
    return ret;
}
