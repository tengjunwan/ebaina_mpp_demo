/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
#include <signal.h>
#include "sample_comm.h"
#include "sample_ipc.h"
#include "sample_aibnr.h"

#include "rtsp_server_api.h"

static td_void sample_aiisp_usage(const char *prg_name)
{
    printf("usage : %s <index> <rtsp_enable> [sns_type]\n", prg_name);
    printf("index:\n");
    printf("    (0) aibnr line mode : base mode, has reference frame\n");
    printf("    (1) aibnr line pro mode\n");
    printf("rtsp_enable:\n");
    printf("\t  0) disable rtsp : venc encode to local file (recording).\n");
    printf("\t  1) enable rtsp  : venc encode to rtsp stream (no local recording).\n");
    printf("sns_type: (optional)\n");
    printf("\t  NOTE: If sns_type is specified, sensor0~sensor1 will ALL use this type.\n");
    printf("\t        If not specified, sensor0~sensor1 use default values from Makefile.\n");
    printf("\t  4)  SC4336P_MIPI_4M_30FPS_10BIT\n");
    printf("\t  5)  SC4336P_MIPI_3M_30FPS_10BIT\n");
    printf("\t  6)  SC4336P_MIPI_2M_30FPS_10BIT\n");
    printf("\t  7)  SC450AI_MIPI_4M_30FPS_10BIT\n");
    printf("\t  9)  SC500AI_MIPI_5M_30FPS_10BIT\n");
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

static td_s32 sample_aiisp_msg_proc_vb_pool_share(td_s32 pid)
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
        ret = ss_mpi_isp_mem_share((td_s32)i, pid);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_isp_mem_share vi_pipe %u, pid %d fail\n", i, pid);
        }
    }

    return TD_SUCCESS;
}

static td_void sample_aiisp_msg_proc_vb_pool_unshare(td_s32 pid)
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
        ret = ss_mpi_isp_mem_unshare((td_s32)i, pid);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_isp_mem_unshare vi_pipe %u, pid %d fail\n", i, pid);
        }
    }
}

static td_s32 sample_aiisp_ipc_msg_proc(const sample_ipc_msg_req_buf *msg_req_buf,
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
            ret = sample_aiisp_msg_proc_vb_pool_share(msg_req_buf->msg_data.pid);
            msg_res_buf->msg_type = SAMPLE_MSG_TYPE_VB_POOL_SHARE_RES;
            msg_res_buf->msg_data.is_req_success = (ret == TD_SUCCESS) ? TD_TRUE : TD_FALSE;
            break;
        }
        case SAMPLE_MSG_TYPE_VB_POOL_UNSHARE_REQ: {
            if (msg_res_buf == TD_NULL) {
                return TD_FAILURE;
            }
            sample_aiisp_msg_proc_vb_pool_unshare(msg_req_buf->msg_data.pid);
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

static td_s32 sample_aiisp_execute_case(td_u32 case_index)
{
    td_s32 ret = TD_FAILURE;
    sample_aibnr_param aibnr_param = {0};

    switch (case_index) {
        case 0: /* 0 aibnr line mode */
            aibnr_param.is_wdr_mode = TD_FALSE;
            aibnr_param.ref_mode = OT_AIBNR_REF_MODE_NORM;
            aibnr_param.is_blend = TD_FALSE;
            aibnr_param.is_pro = TD_FALSE;
            ret = sample_aibnr(&aibnr_param);
            break;
        case 1: /* 1 aibnr line pro mode */
            aibnr_param.is_wdr_mode = TD_FALSE;
            aibnr_param.ref_mode = OT_AIBNR_REF_MODE_NORM;
            aibnr_param.is_blend = TD_FALSE;
            aibnr_param.is_pro = TD_TRUE;
            ret = sample_aibnr(&aibnr_param);
            break;
        default:
            break;
    }

    return ret;
}

#ifdef __LITEOS__
td_s32 app_main(td_s32 argc, td_char *argv[])
#else
td_s32 main(td_s32 argc, td_char *argv[])
#endif
{
    td_s32 ret = TD_FAILURE;
    td_u32 index;
    td_s32 push_rtsp = 0;
    td_char *para_stop;

    if (argc < 0x3 || argc > 0x4) { /* 2:arg num */
        sample_aiisp_usage(argv[0]);
        goto exit;
    }

    if (!strncmp(argv[1], "-h", 2)) { /* 2:arg num */
        sample_aiisp_usage(argv[0]);
        goto exit;
    }

    if (strlen(argv[1]) > 2 || strlen(argv[1]) == 0 || !check_digit(argv[1][0]) || /* 2:arg len */
        (strlen(argv[1]) == 2 && (!check_digit(argv[1][1]) || argv[1][0] == '0'))) { /* 2:arg len */
        sample_aiisp_usage(argv[0]);
        goto exit;
    }

#ifndef __LITEOS__
    sample_register_sig_handler(sample_aiisp_handle_sig);
#endif

    if (sample_ipc_server_init(sample_aiisp_ipc_msg_proc) != TD_SUCCESS) {
        printf("sample_ipc_server_init failed!!!\n");
    }
    index = (td_u32)strtol(argv[1], &para_stop, 10); /* 1 idx , 10 dec */
    if (index > 10) { /* 10:arg len */
        sample_aiisp_usage(argv[0]);
        goto server_deinit;
    }

    push_rtsp = (td_u32)strtol(argv[2], &para_stop, 10);
    if ((para_stop == argv[2]) || (*para_stop) != '\0') {
        sample_aiisp_usage(argv[0]);
        goto server_deinit;
    }
    sample_comm_venc_set_push_rtsp((push_rtsp == 1) ? TD_TRUE : TD_FALSE);
    if (push_rtsp){
        const char *rtsp_ip = "0.0.0.0";
        rtsp_server_start(rtsp_ip, 554);
        rtsp_session_create(0, 1);  //ven0 h265
    }

    if (argc == 4) {
        sample_sns_type sns_type;
        sns_type = (sample_sns_type)strtol(argv[3], &para_stop, 10);
        if ((para_stop == argv[3]) || (*para_stop) != '\0') {
            sample_aiisp_usage(argv[0]);
            goto server_deinit;
        }
        sample_set_all_sns_type(sns_type);
    }

    ret = sample_aiisp_execute_case(index);
    if ((ret == TD_SUCCESS) && (sample_aiisp_get_sig() == 0)) {
        sample_print("\033[0;32mprogram exit normally!\033[0;39m\n");
    } else {
        sample_print("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    if (push_rtsp){
        rtsp_server_stop();
    }

server_deinit:
    sample_ipc_server_deinit();

exit:
#ifdef __LITEOS__
    return ret;
#else
    exit(ret);
#endif
}