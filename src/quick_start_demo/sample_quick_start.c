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

#define VENC_MAX_CHN_NUM 8

#define TRACE_TIMESTAMP 1
#define ENABLE_VENC 0
#define RUNNING_DIRECT 1
// '0' online; '3' offline
#define RUNNING_OPTION '3'
#define BUFFER_SIZE 128
#define CHN_DEPTH 2

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

static sampe_sys_cfg g_quick_start_sys_cfg = {
    .route_num = 1,
    .mode_type = OT_VI_OFFLINE_VPSS_OFFLINE,
    .vpss_wrap_en = TD_FALSE,
    .vpss_wrap_size = 3200 * 128 *1.5,
};

#if ENABLE_VENC
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
#endif

#if TRACE_TIMESTAMP
#define CMD_RESULT_BUF_SIZE 512
#define MAX_INDEX_NUM 64
td_char timestamp_buf[MAX_INDEX_NUM][CMD_RESULT_BUF_SIZE] = {0};
td_s32 time_index = 0;

static td_s32 execute_cmd(const td_char *cmd, td_u32 cmd_len, td_char *result, td_u32 res_len)
{
    td_s32 iRet = TD_FAILURE;
    td_char buf_ps[CMD_RESULT_BUF_SIZE];
    td_char ps[CMD_RESULT_BUF_SIZE] = {0};
    FILE *ptr;

    if ((cmd == TD_NULL) || (result == TD_NULL) ||
        (cmd_len > CMD_RESULT_BUF_SIZE) ||
        (res_len > CMD_RESULT_BUF_SIZE)) {
        return iRet;
    }

    if ((strcpy_s(ps, CMD_RESULT_BUF_SIZE, cmd) == EOK) &&
        (ptr = popen(ps, "r")) != NULL) {
        while (fgets(buf_ps, sizeof(buf_ps), ptr) != NULL) {
            if ((strcat_s(result, CMD_RESULT_BUF_SIZE, buf_ps) != EOK) ||
                (strlen(result) > CMD_RESULT_BUF_SIZE)) {
                break;
            }
        }
        pclose(ptr);
        ptr = NULL;
        iRet = TD_SUCCESS;
    }

    return iRet;
}

static void print_echo_to_buf(td_char* name, td_u32 len)
{
    static td_char flag = 0;
    static td_u64 last_pts = 0;
    td_u64 pts;
    td_u64 gap = 0;

    if ((name == TD_NULL) || (len > CMD_RESULT_BUF_SIZE)) {
        return;
    }
    ss_mpi_sys_get_cur_pts(&pts);

    if (flag == 0) {
        flag = 1;
        gap = 0;
    } else {
        gap = pts - last_pts;
    }
    last_pts = pts;

    if (sprintf_s(timestamp_buf[time_index], CMD_RESULT_BUF_SIZE - 1,
                  "echo %s pts is: %llu, gap: %llu > /dev/kmsg", name, pts, gap) < 0) {
        return;
    }

    time_index++;
}

void print_timestamp(td_char* name)
{
    print_echo_to_buf(name, CMD_RESULT_BUF_SIZE);
}
void print_timestamp_with_pts(td_char* name, td_u64 frame_pts)
{
    td_s32 ret;
    td_char name_pts[CMD_RESULT_BUF_SIZE] = {0};
    ret = sprintf_s(name_pts, CMD_RESULT_BUF_SIZE - 1, "%s_%llu", name, frame_pts);
    if (ret < 0) {
        return;
    }
    print_echo_to_buf(name_pts, CMD_RESULT_BUF_SIZE);
}

static void dump_all_timestamp_to_kmsg(void)
{
    td_s32 i;
    td_char result[CMD_RESULT_BUF_SIZE];

    for (i = 0;i < time_index; i++) {
        if (memset_s(result, CMD_RESULT_BUF_SIZE, 0, CMD_RESULT_BUF_SIZE) != EOK) {
            printf("memset_s error \n");
        }
        execute_cmd(timestamp_buf[i], CMD_RESULT_BUF_SIZE, result, CMD_RESULT_BUF_SIZE);
    }
}
#else
void print_timestamp(char* name)
{
}
void print_timestamp_with_pts(char* name, td_u64 frame_pts)
{
}
static void dump_all_timestamp_to_kmsg(void)
{
}
#endif


/* define SAMPLE_MEM_SHARE_ENABLE, when use tools to dump YUV/RAW. */
#ifdef SAMPLE_MEM_SHARE_ENABLE
td_void sample_quick_start_init_mem_share(td_void)
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

static td_void sample_quick_start_get_pipe_wrap_line(sample_vi_cfg vi_cfg[], td_s32 route_num)
{
    ot_vi_wrap_out_param out_param;
    ot_vi_wrap_in_param in_param;
    td_s32 i;

    if (g_quick_start_sys_cfg.mode_type == OT_VI_ONLINE_VPSS_ONLINE ||
        g_quick_start_sys_cfg.mode_type == OT_VI_ONLINE_VPSS_OFFLINE) {
        return;
    }

    (td_void)memcpy_s(&in_param, sizeof(ot_vi_wrap_in_param), &g_vi_wrap_param, sizeof(ot_vi_wrap_in_param));
    if (g_quick_start_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_OFFLINE) {
        in_param.wrap_param[0].slave_param.vi_vpss_online = TD_FALSE;
        in_param.wrap_param[1].slave_param.vi_vpss_online = TD_FALSE;
    }
    in_param.pipe_num = (td_u32)route_num;
#ifdef OT_FPGA
    in_param.wrap_param[0].frame_rate_100x = 1500; /* 1500: 15fps*100 */
    in_param.wrap_param[1].frame_rate_100x = 1500; /* 1500: 15fps*100 */
    in_param.wrap_param[0].slave_param.vpss_venc_wrap = TD_FALSE;
    in_param.wrap_param[1].slave_param.vpss_venc_wrap = TD_FALSE;
#endif
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &in_param.wrap_param[0].pipe_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &in_param.wrap_param[0].slave_param.vpss_chn0_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &in_param.wrap_param[0].slave_param.large_stream_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR1_TYPE, &in_param.wrap_param[1].pipe_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR1_TYPE, &in_param.wrap_param[1].slave_param.vpss_chn0_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR1_TYPE, &in_param.wrap_param[1].slave_param.large_stream_size);
    in_param.wrap_param[0].full_lines_std = in_param.wrap_param[0].pipe_size.height;
    in_param.wrap_param[1].full_lines_std = in_param.wrap_param[1].pipe_size.height;
    if (ss_mpi_sys_get_vi_wrap_buffer_line(&in_param, &out_param) != TD_SUCCESS) {
        sample_print("sample_vie get wrap line fail, pipe_wrap not enable!\n");
        return;
    }

    for (i = 0; i < route_num; i++) {
        vi_cfg[i].pipe_info[0].wrap_attr.buf_line = out_param.buf_line[i];
        vi_cfg[i].pipe_info[0].wrap_attr.enable = TD_TRUE;
    }
}

static td_s32 sample_quick_start_sys_init(td_void)
{
    ot_vb_cfg vb_cfg;
    td_u32 supplement_config = OT_VB_SUPPLEMENT_BNR_MOT_MASK | OT_VB_SUPPLEMENT_MOTION_DATA_MASK;

    sample_comm_sys_get_default_vb_cfg(&g_vb_param, &vb_cfg);
    /* prepare vpss wrap vb */
    if (g_quick_start_sys_cfg.vpss_wrap_en) {
        vb_cfg.common_pool[SAMPLE_VIE_POOL_NUM].blk_cnt = 1;
        vb_cfg.common_pool[SAMPLE_VIE_POOL_NUM].blk_size = g_quick_start_sys_cfg.vpss_wrap_size;
    }
    if (sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_config) != TD_SUCCESS) {
        return TD_FAILURE;
    }

#ifdef SAMPLE_MEM_SHARE_ENABLE
    sample_quick_start_init_mem_share();
#endif

    if (sample_comm_vi_set_vi_vpss_mode(g_quick_start_sys_cfg.mode_type, OT_VI_AIISP_MODE_DEFAULT) != TD_SUCCESS) {
        goto sys_exit;
    }

    return TD_SUCCESS;
sys_exit:
    sample_comm_sys_exit();
    return TD_FAILURE;
}

static td_s32 sample_vpss_set_wrap_grp_int_attr(ot_vi_vpss_mode_type mode_type, td_bool wrap_en, td_u32 max_height)
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

static td_s32 sample_quick_start_start_vpss(ot_vpss_grp grp, sample_vpss_cfg *vpss_cfg)
{
    td_s32 ret;
    sample_vpss_chn_attr vpss_chn_attr = {0};

    if (grp == 0) {
        ret = sample_vpss_set_wrap_grp_int_attr(g_quick_start_sys_cfg.mode_type,
            vpss_cfg->wrap_attr[0].enable, vpss_cfg->grp_attr.max_height);
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

static td_void sample_quick_start_stop_vpss(ot_vpss_grp grp)
{
    td_bool chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_FALSE, TD_FALSE};

    sample_common_vpss_stop(grp, chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
}

#if ENABLE_VENC
static td_s32 sample_quick_start_start_venc(ot_venc_chn venc_chn[], ot_size venc_size[], size_t size, td_u32 chn_num)
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

    return TD_SUCCESS;

exit:
    for (i = i - 1; i >= 0; i--) {
        sample_comm_venc_stop(venc_chn[i]);
    }
    return TD_FAILURE;
}

static td_void sample_quick_start_stop_venc(ot_venc_chn venc_chn[], size_t size, td_u32 chn_num)
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

static td_s32 sample_quick_start_start_and_bind_venc(ot_vpss_grp vpss_grp[], sample_vpss_cfg *vpss_cfg, size_t size,
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
    if (g_quick_start_sys_cfg.vpss_wrap_en) {
        chn_num *= 2; /* 2 chns */
    }
    ret = sample_quick_start_start_venc(venc_chn, venc_size,
        sizeof(venc_chn) / sizeof(venc_chn[0]), chn_num); /* 2 chns */
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_bind_venc(vpss_grp[i], OT_VPSS_CHN0, venc_chn[i * 2]); /* 2 chns */
        if (g_quick_start_sys_cfg.vpss_wrap_en) {
            sample_comm_vpss_bind_venc(vpss_grp[i], OT_VPSS_CHN1, venc_chn[i * 2 + 1]); /* 2 chns */
        }
    }

    return TD_SUCCESS;
}

static td_void sample_quick_start_stop_and_unbind_venc(ot_vpss_grp vpss_grp[], size_t size, td_u32 grp_num)
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
        if (g_quick_start_sys_cfg.vpss_wrap_en) {
            sample_comm_vpss_un_bind_venc(vpss_grp[i], OT_VPSS_CHN1, venc_chn[i * 2 + 1]); /* 2 chns */
        }
    }

    chn_num = grp_num;
    if (g_quick_start_sys_cfg.vpss_wrap_en) {
        chn_num *= 2; /* 2 chns */
    }
    sample_quick_start_stop_venc(venc_chn, sizeof(venc_chn) / sizeof(venc_chn[0]), grp_num * 2); /* 2 chns */
}
#endif

/* when get error times over than 10, quiet the output */
#define MAX_TIMES 20

#define SAMPLE_RETURN_CONTINUE  1
#define SAMPLE_RETURN_BREAK     2

#if ENABLE_VENC
static td_s32 sample_venc_get_stream_from_one_channl(td_s32 index, td_bool print_flag)
{
    ot_venc_stream stream;
    ot_venc_chn_status stat;

    /* step 2.1 : query how many packs in one-frame stream. */
    if (memset_s(&stream, sizeof(stream), 0, sizeof(stream)) != EOK) {
        printf("call memset_s error\n");
    }

    if (ss_mpi_venc_query_status(index, &stat) != TD_SUCCESS) {
        printf("ss_mpi_venc_query_status chn[%d] failed !\n", index);
        return SAMPLE_RETURN_BREAK;
    }
    if (stat.cur_packs == 0) {
        return SAMPLE_RETURN_CONTINUE;
    }
    /* step 2.3 : malloc corresponding number of pack nodes. */
    stream.pack = (ot_venc_pack *)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
    if (stream.pack == TD_NULL) {
        return SAMPLE_RETURN_BREAK;
    }

    /* step 2.4 : call mpi to get one-frame stream */
    stream.pack_cnt = stat.cur_packs;
    if (ss_mpi_venc_get_stream(index, &stream, TD_TRUE) != TD_SUCCESS) {
        free(stream.pack);
        stream.pack = TD_NULL;
        printf("ss_mpi_venc_get_stream failed !\n");
        return SAMPLE_RETURN_BREAK;
    }

    if (print_flag) {
        for (td_u32 i = 0; i < stat.cur_packs; i++) {
            printf("---------------------> pack %d pts is %lld\n", i, stream.pack[i].pts);
            char str_buf[BUFFER_SIZE] = {0};
            td_s32 ret = snprintf_s(str_buf, sizeof(str_buf), sizeof(str_buf) - 1, \
                "----- pack %d pts is %lld after get the stream in the step2.4", i, stream.pack[i].pts);
            if ((ret < 0) || (ret > (BUFFER_SIZE - 1))) {
                return SAMPLE_RETURN_BREAK;
            }
            print_timestamp(str_buf);
        }
    }

    /* step 2.6 : release stream */
    if (ss_mpi_venc_release_stream(index, &stream) != TD_SUCCESS) {
        printf("ss_mpi_venc_release_stream failed!\n");
        free(stream.pack);
        stream.pack = TD_NULL;
        return SAMPLE_RETURN_BREAK;
    }

    /* step 2.7 : free pack nodes */
    free(stream.pack);
    stream.pack = TD_NULL;
    return TD_SUCCESS;
}
#endif

static void sample_quick_start_init_depth(void)
{
    td_s32 ret;
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vi_chn_attr chn_attr;

    ret = ss_mpi_vi_get_chn_attr(vi_pipe, vi_chn, &chn_attr);
    if (ret != TD_SUCCESS) {
        printf("============== ss_mpi_vi_get_chn_attr, ret:0x%x\n", ret);
    }

    chn_attr.depth = CHN_DEPTH;
    ret = ss_mpi_vi_set_chn_attr(vi_pipe, vi_chn, &chn_attr);
    if (ret != TD_SUCCESS) {
        printf("============== ss_mpi_vi_set_chn_attr, ret:0x%x\n", ret);
    }

    const ot_vpss_grp vpss_grp = 0;
    const ot_vpss_chn vpss_chn = 0;
    ot_vpss_chn_attr vpss_chn_attr;
    ret = ss_mpi_vpss_get_chn_attr(vpss_grp, vpss_chn, &vpss_chn_attr);
    if (ret != TD_SUCCESS) {
        printf("============== ss_mpi_vpss_get_chn_attr, ret:0x%x\n", ret);
    }

    vpss_chn_attr.depth = CHN_DEPTH;
    ret = ss_mpi_vpss_set_chn_attr(vpss_grp, vpss_chn, &vpss_chn_attr);
    if (ret != TD_SUCCESS) {
        printf("============== ss_mpi_vpss_set_chn_attr, ret:0x%x\n", ret);
    }
}

static td_bool g_get_frame = TD_TRUE;
#define OVER_TIME_MAX 400
#define SLEEP_UTIME 5000

#if ENABLE_VENC
static void sample_quick_start_get_venc_frame(td_void)
{
    td_s32 ret = 0;
    td_s32 overtime = 0;

    while (g_get_frame) {
        ret = sample_venc_get_stream_from_one_channl(0, TD_TRUE);
        if (ret != TD_SUCCESS && overtime < OVER_TIME_MAX) {
            usleep(SLEEP_UTIME);
            overtime++;
            continue;
        }

        if (overtime >= OVER_TIME_MAX) {
            printf("sample_venc_get_stream_from_one_channl failed ret=0x%x\n", ret);
        }
        break;
    }
}
#endif

static void sample_quick_start_get_vi_frame(td_void)
{
    ot_video_frame_info frame_info = {0};
    td_s32 wait_time = 1000; /* wait 1000 us */
    td_s32 overtime = 0;
    td_s32 ret;

    print_timestamp("ss_mpi_vi_get_chn_frame_start");
    while (g_get_frame && g_quick_start_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_OFFLINE) {
        ret = ss_mpi_vi_get_chn_frame(0, 0, &frame_info, wait_time);
        if (ret != TD_SUCCESS && overtime < OVER_TIME_MAX) {
            usleep(SLEEP_UTIME);
            overtime++;
            continue;
        }

        if (overtime >= OVER_TIME_MAX) {
            printf("============== ss_mpi_vi_get_chn_frame failed ret=0x%x\n", ret);
        }

        print_timestamp_with_pts("ss_mpi_vi_get_chn_frame_done", frame_info.video_frame.pts);

        ss_mpi_vi_release_chn_frame(0, 0, &frame_info);
        break;
    }
}

static void sample_quick_start_get_vpss_frame(td_void)
{
    ot_video_frame_info frame_info = {0};
    td_s32 wait_time = 1000; /* wait 1000 us */
    td_s32 overtime = 0;
    td_s32 ret;

    print_timestamp("ss_mpi_vpss_get_chn_frame_start");
    while (g_get_frame && g_quick_start_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_OFFLINE) {
        ret = ss_mpi_vpss_get_chn_frame(0, 0, &frame_info, wait_time);
        if (ret != TD_SUCCESS && overtime < OVER_TIME_MAX) {
            usleep(SLEEP_UTIME);
            overtime++;
            continue;
        }

        if (overtime >= OVER_TIME_MAX) {
            printf("============== ss_mpi_vpss_get_chn_frame failed ret=0x%x\n", ret);
        }

        print_timestamp_with_pts("ss_mpi_vpss_get_chn_frame_done", frame_info.video_frame.pts);
        ss_mpi_vpss_release_chn_frame(0, 0, &frame_info);
        break;
    }
}

void* sample_quick_start_get_first_frame(td_void *param)
{
    printf("--------------- g_quick_start_sys_cfg.mode_type: %d\n", g_quick_start_sys_cfg.mode_type);

    sample_quick_start_get_vi_frame();
    sample_quick_start_get_vpss_frame();
#if ENABLE_VENC
   print_timestamp("venc_stream_start_first_time_start");
   sample_quick_start_get_venc_frame();
   print_timestamp("venc_stream_end_first_time_done");

   print_timestamp("venc_stream_start_second_time_start");
   sample_quick_start_get_venc_frame();
   print_timestamp("venc_stream_end_second_time_done");
#endif
    return NULL;
}

static pthread_t get_frameinfo_thread_id = 0;

static td_s32 sample_quick_start_vi_get_frame(void)
{
    td_s32 ret;
    pthread_t thread_id = 0;
    g_get_frame = TD_TRUE;
    ret = pthread_create(&thread_id, TD_NULL, sample_quick_start_get_first_frame, NULL);
    if (ret != TD_SUCCESS) {
        printf("vi create run be send frame thread failed!\n");
        return TD_FAILURE;
    }
    get_frameinfo_thread_id = thread_id;
    return ret;
}

static void sample_quick_start_get_sns_type(void)
{
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &g_vb_param.vb_size[0]);
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &g_vb_param.vb_size[1]);
}

static void sample_start_vi_failed(sample_vi_cfg *vi_cfg, td_s32 route_num)
{
    td_s32 j;
    for (j = route_num - 1; j >= 0; j--) {
        sample_comm_vi_stop_vi(&vi_cfg[j]);
    }
    sample_comm_sys_exit();
}

static td_s32 sample_quick_start_start_route(sample_vi_cfg *vi_cfg, sample_vpss_cfg *vpss_cfg, td_s32 route_num)
{
    td_s32 i;
    ot_vpss_grp vpss_grp[SAMPLE_VIE_MAX_ROUTE_NUM] = {0, 1, 2, 3};

    if (sample_quick_start_vi_get_frame() != TD_SUCCESS) {
        printf("sample_quick_start_vi_get_frame failed\n");
    }

    print_timestamp("sample_vio_start_route_start");
    sample_quick_start_get_sns_type();
    if (sample_quick_start_sys_init() != TD_SUCCESS) {
        return TD_FAILURE;
    }

    print_timestamp("route_sys_start_end");
    for (i = 0; i < route_num; i++) {
        // skip the sensor init during the isp init stage, the sensor init put into uboot stage now
#ifdef CONFIG_SENSOR_INIT_IN_UBOOT
        vi_cfg[i].pipe_info[0].isp_quick_start = TD_TRUE;
        vi_cfg[i].sns_info.sns_clk_rst_en = TD_FALSE;
#endif
        if (sample_comm_vi_start_vi(&vi_cfg[i]) != TD_SUCCESS) {
            goto start_vi_failed;
        }
    }

    print_timestamp("route_vi_start_end");
    for (i = 0; i < route_num; i++) {
        sample_comm_vi_bind_vpss(i, 0, vpss_grp[i], 0);
    }

    for (i = 0; i < route_num; i++) {
        if (sample_quick_start_start_vpss(vpss_grp[i], vpss_cfg) != TD_SUCCESS) {
            goto start_vpss_failed;
        }
    }
    print_timestamp("route_vpss_start_end");

#if ENABLE_VENC
    if (sample_quick_start_start_and_bind_venc(vpss_grp, vpss_cfg, SAMPLE_VIE_MAX_ROUTE_NUM, route_num) != TD_SUCCESS) {
        goto start_venc_failed;
    }
    print_timestamp("route_venc_start_end");
#endif

    sample_quick_start_init_depth();
    print_timestamp("sample_vio_start_route_done");
    pthread_join(get_frameinfo_thread_id, TD_NULL);

    return TD_SUCCESS;

#if ENABLE_VENC
start_venc_failed:
#endif
start_vpss_failed:
    for (i = i - 1; i >= 0; i--) {
        sample_quick_start_stop_vpss(vpss_grp[i]);
    }
    for (i = 0; i < route_num; i++) {
        sample_comm_vi_un_bind_vpss(i, 0, vpss_grp[i], 0);
    }
start_vi_failed:
    sample_start_vi_failed(vi_cfg, i);
    return TD_FAILURE;
}

static td_void sample_quick_start_stop_route(sample_vi_cfg *vi_cfg, td_s32 route_num)
{
    td_s32 i;
    ot_vpss_grp vpss_grp[SAMPLE_VIE_MAX_ROUTE_NUM] = {0, 1, 2, 3};

#if ENABLE_VENC
    sample_quick_start_stop_and_unbind_venc(vpss_grp, SAMPLE_VIE_MAX_ROUTE_NUM, route_num);
#endif
    for (i = 0; i < route_num; i++) {
        sample_quick_start_stop_vpss(vpss_grp[i]);
        sample_comm_vi_un_bind_vpss(i, 0, vpss_grp[i], 0);
        sample_comm_vi_stop_vi(&vi_cfg[i]);
    }
    sample_comm_sys_exit();
}

static td_void sample_quick_start_print_vi_mode_list(td_bool is_wdr_mode)
{
#if (RUNNING_DIRECT == 0)
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
#endif
}

static td_void sample_quick_start_get_vi_vpss_mode_by_char(td_char ch, td_bool is_wdr)
{
    switch (ch) {
        case '0':
            g_quick_start_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_ONLINE;
            g_vb_param.blk_num[0] = 0; /* raw_vb num 0 */
            g_vb_param.blk_num[1] = 0; /* yuv_vb num 0 */
            break;
        case '1':
            g_quick_start_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_OFFLINE;
            g_vb_param.blk_num[0] = 0; /* raw_vb num 0 */
            g_vb_param.blk_num[1] = 8; /* yuv_vb num 8 */
            break;
        case '2':
            g_quick_start_sys_cfg.mode_type = OT_VI_OFFLINE_VPSS_ONLINE;
            g_vb_param.blk_num[0] = is_wdr ? 6 : 0; /* raw_vb num 6 or 3 */
            g_vb_param.blk_num[1] = 0; /* yuv_vb num 0 */
            break;
        case '3':
            g_quick_start_sys_cfg.mode_type = OT_VI_OFFLINE_VPSS_OFFLINE;
            g_vb_param.blk_num[0] = is_wdr ? 6 : 0; /* raw_vb num 6 or 3 */
            break;
        default:
            g_quick_start_sys_cfg.mode_type = OT_VI_ONLINE_VPSS_ONLINE;
            g_vb_param.blk_num[0] = 0; /* raw_vb num 0 */
            g_vb_param.blk_num[1] = 0; /* yuv_vb num 0 */
            break;
    }
}

static td_void sample_quick_start_get_vi_vpss_mode(td_bool is_wdr_mode)
{
#if RUNNING_DIRECT
    td_char input[3] = {0}; /* max_len: 3 */
#else
    td_char ch = '0';
    td_char end_ch = '4';
    td_char input[3] = {0}; /* max_len: 3 */
    td_s32 max_len = 3; /* max_len: 3 */

#endif
    sample_quick_start_print_vi_mode_list(is_wdr_mode);

#if RUNNING_DIRECT
    printf("using the option %c default.\n", (td_char)RUNNING_OPTION);
    input[0] = (td_char)RUNNING_OPTION;
#else
    while (g_sig_flag == 0) {
        if (gets_s(input, max_len) != TD_NULL && strlen(input) == 1 && input[0] >= ch && input[0] <= end_ch) {
            break;
        } else {
            printf("\nInvalid param, please enter again!\n\n");
            sample_quick_start_print_vi_mode_list(is_wdr_mode);
        }
        (td_void)fflush(stdin);
    }
#endif

    sample_quick_start_get_vi_vpss_mode_by_char(input[0], is_wdr_mode);
}

static td_void sample_quick_start_vpss_get_wrap_cfg(sampe_sys_cfg *quick_start_sys_cfg, sample_vpss_cfg *vpss_cfg)
{
    td_u32 full_lines_std;
    if (quick_start_sys_cfg->vpss_wrap_en) {
        if (SENSOR0_TYPE == HY006_3814_0011_MIPI_4M_30FPS_10BIT ||
            SENSOR0_TYPE == HY006_3814_0011_MIPI_3M_30FPS_10BIT ||
            SENSOR0_TYPE == HY006_3814_0011_MIPI_2M_30FPS_10BIT ||
            SENSOR0_TYPE == HY006_3814_0011_MIPI_4M_30FPS_10BIT_WDR2TO1 ||
            SENSOR0_TYPE == SC4336P_MIPI_4M_30FPS_10BIT ||
            SENSOR0_TYPE == SC4336P_MIPI_3M_30FPS_10BIT ||
            SENSOR0_TYPE == SC4336P_MIPI_2M_30FPS_10BIT) {
            full_lines_std = 1500; /* full_lines_std: 1500 */
        } else if (SENSOR0_TYPE == SC450AI_MIPI_4M_30FPS_10BIT || SENSOR0_TYPE == SC450AI_MIPI_4M_30FPS_10BIT_WDR2TO1) {
            full_lines_std = 1585; /* full_lines_std: 1585 */
        } else if (SENSOR0_TYPE == SC500AI_MIPI_5M_30FPS_10BIT || SENSOR0_TYPE == SC500AI_MIPI_5M_30FPS_10BIT_WDR2TO1) {
            full_lines_std = 1700; /* full_lines_std: 1700 */
        } else {
            quick_start_sys_cfg->vpss_wrap_en = TD_FALSE;
            vpss_cfg->wrap_attr[0].enable = TD_FALSE;
            return;
        }
        (td_void)sample_comm_vpss_get_wrap_cfg(vpss_cfg->chn_attr, quick_start_sys_cfg->mode_type, full_lines_std,
            &vpss_cfg->wrap_attr[0]);
        quick_start_sys_cfg->vpss_wrap_size = vpss_cfg->wrap_attr[0].buf_size;
    }
}

static td_s32 sample_quick_start_all_mode(td_void)
{
    sample_vi_cfg vi_cfg[1];
    sample_vpss_cfg vpss_cfg;
    sample_sns_type sns_type = SENSOR0_TYPE;

    print_timestamp("sample_vio_all_mode cfg start");
    sample_quick_start_get_vi_vpss_mode(TD_FALSE);
    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg[0]);
    sample_quick_start_get_pipe_wrap_line(vi_cfg, 1);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &vpss_cfg, g_quick_start_sys_cfg.vpss_wrap_en);
    sample_quick_start_vpss_get_wrap_cfg(&g_quick_start_sys_cfg, &vpss_cfg);
    print_timestamp("sample_vio_all_mode cfg end");
    if (sample_quick_start_start_route(vi_cfg, &vpss_cfg, g_quick_start_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_quick_start_stop_route(vi_cfg, g_quick_start_sys_cfg.route_num);
    return TD_SUCCESS;
}


static td_void sample_quick_start_handle_sig(td_s32 signo)
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

static td_s32 sample_quick_start_execute_case(td_u32 case_index)
{
    td_s32 ret;

    switch (case_index) {
        case 0: /* 0 all mode route */
            ret = sample_quick_start_all_mode();
            break;
        default:
            ret = TD_FAILURE;
            break;
    }

    return ret;
}

static td_void sample_quick_start_usage(const char *prg_name)
{
    printf("usage : %s \n", prg_name);
    printf("Description : %s will record the timestamp to dmesg and auto exit after running done!\n", prg_name);
}

#ifdef __LITEOS__
td_s32 app_main(td_s32 argc, td_char *argv[])
#else
td_s32 main(td_s32 argc, td_char *argv[])
#endif
{
    td_s32 ret;
    print_timestamp("sample_main_enter");

    if (argc > 1) { /* 1:arg num */
        sample_quick_start_usage(argv[0]);
        return TD_FAILURE;
    }

#ifndef __LITEOS__
    sample_register_sig_handler(sample_quick_start_handle_sig);
#endif
    print_timestamp("sample_main_sighandle_end");
    ret = sample_quick_start_execute_case(0);
    if ((ret == TD_SUCCESS) && (g_sig_flag == 0)) {
        printf("\033[0;32mprogram exit normally!\033[0;39m\n");
    } else {
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    print_timestamp("sample_main_exit");
    dump_all_timestamp_to_kmsg();
    return ret;
}
