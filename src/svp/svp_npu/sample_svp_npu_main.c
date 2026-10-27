/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "securec.h"
#include "sample_svp_npu_process.h"
#include "sns_type.h"

#define SAMPLE_SVP_NPU_ARG_NUM_TWO 2
#define SAMPLE_SVP_NPU_ARG_NUM_THREE 3
#define SAMPLE_SVP_NPU_ARG_NUM_FOUR 4
#define SAMPLE_SVP_NPU_ARG_NUM_FIVE 5
#define SAMPLE_SVP_NPU_ARG_IDX_TWO 2
#define SAMPLE_SVP_NPU_CMP_STR_NUM 2
#define SAMPLE_SVP_NPU_ARG_STR_LEN 2
#define SAMPLE_SVP_NPU_ARG_IDX_BASE 10

static char **g_npu_cmd_argv = TD_NULL;

/*
 * function : to process abnormal case
 */
#ifndef __LITEOS__
static td_void sample_svp_npu_handle_sig(td_s32 signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        sample_svp_npu_acl_handle_sig();
    }
}
#endif

/*
 * function : show usage
 */
static td_void sample_svp_npu_usage(const td_char *prg_name)
{
    printf("Usage : %s <index> [sub_idx] <rtsp_enable> [sns_type] \n", prg_name);
    printf("index:\n");
    printf("\t 0) svp_acl_resnet50.\n");
    printf("\t 1) svp_acl_resnet50_multi_thread.\n");
    printf("\t 2) svp_acl_resnet50_dynamic_batch_with_mem_cached.\n");
    printf("\t 3) svp_acl_lstm.\n");
    printf("\t 4) svp_acl_preemption.\n");
    printf("\t a) [8] yolov8.(VI->VPSS->SVP_NPU->VGS->VENC).\n");
    printf("sub_idx: \n");
    printf("\t  NOTE: only required for index a.\n");
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

static td_s32 sample_svp_npu_case_with_one_arg(td_char idx)
{
    td_s32 ret = TD_SUCCESS;

    switch (idx) {
        case '0':
            sample_svp_npu_acl_resnet50();
            break;
        case '1':
            sample_svp_npu_acl_resnet50_multi_thread();
            break;
        case '2':
            sample_svp_npu_acl_resnet50_dynamic_batch();
            break;
        case '3':
            sample_svp_npu_acl_lstm();
            break;
        case '4':
            sample_svp_npu_acl_preemption();
            break;
        default:
            ret = TD_FAILURE;
            break;
    }
    return ret;
}

static td_s32 sample_svp_npu_case_with_two_args(td_char idx1, td_char idx2)
{
    td_s32 ret = TD_SUCCESS;
    if ((idx1 == 'a' || idx1 == 'A') && !(idx2 == '8')) {
        return TD_FAILURE;
    }
    switch (idx1) {
        case 'A':
        case 'a':
            printf("[wtjdebug][sample_svp_npu_main][sample_svp_npu_case_with_two_args]: idx2 = %c\n", idx2);
            sample_svp_npu_acl_e2e_yolo(idx2 - '0'); // wtj debug: equivalent int("8")
            ret = TD_SUCCESS;
            break;
        default:
            ret = TD_FAILURE;
            break;
    }
    return ret;
}

/*
 * function : svp npu sample
 */
#ifdef __LITEOS__
int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    td_s32 ret;
    size_t argv1_len;
    char *end_ptr;
#ifndef __LITEOS__
    struct sigaction sa;
#endif
    if (argc < SAMPLE_SVP_NPU_ARG_NUM_TWO || argc > SAMPLE_SVP_NPU_ARG_NUM_FIVE) {
        sample_svp_npu_usage(argv[0]);
        return TD_FAILURE;
    }

    if (!strncmp(argv[1], "-h", SAMPLE_SVP_NPU_CMP_STR_NUM)) {
        sample_svp_npu_usage(argv[0]);
        return TD_SUCCESS;
    }

    g_npu_cmd_argv = argv;
#ifndef __LITEOS__
    (td_void)memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    sa.sa_handler = sample_svp_npu_handle_sig;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
    argv1_len = strlen(argv[1]);
    if (argv1_len != 1) {
        sample_svp_npu_usage(argv[0]);
        return TD_FAILURE;
    }

    td_char index = argv[1][0];
    if (index == 'a' || index == 'A') {
        if (argc < SAMPLE_SVP_NPU_ARG_NUM_FOUR) {
            sample_svp_npu_usage(argv[0]);
            return TD_FAILURE;
        }
        if (strlen(argv[SAMPLE_SVP_NPU_ARG_IDX_TWO]) != 1) {
            sample_svp_npu_usage(argv[0]);
            return TD_FAILURE;
        }

        td_s32 push_rtsp = (td_u32)strtol(argv[3], &end_ptr, SAMPLE_SVP_NPU_ARG_IDX_BASE);
        if ((end_ptr == argv[3]) || (*end_ptr) != '\0') {
            sample_svp_npu_usage(argv[0]);
            return TD_FAILURE;
        }
        printf("[wtjdebug][sample_svp_npu_main][main]: push_rtsp = %d\n", push_rtsp);
        sample_svp_npu_enable_rtsp(push_rtsp);

        if (argc == SAMPLE_SVP_NPU_ARG_NUM_FIVE) {
            sample_sns_type sns_type;
            sns_type = (td_s32)strtol(argv[4], &end_ptr, SAMPLE_SVP_NPU_ARG_IDX_BASE);
            if ((end_ptr == argv[4]) || (*end_ptr) != '\0') {
                sample_svp_npu_usage(argv[0]);
                return TD_FAILURE;
            }
            printf("[wtjdebug][sample_svp_npu_main][main]: sns_type = %d\n", sns_type);
            sample_set_all_sns_type(sns_type);
        }
        
        // ====================================================================
        // [新增] 启动异步抓图与网络发送线程
        // 注意：需确保在此文件头部或 sample_svp_npu_process.h 中声明了该函数
        // ====================================================================
        sample_svp_capture_threads_init();

        ret = sample_svp_npu_case_with_two_args(*argv[1], *argv[SAMPLE_SVP_NPU_ARG_IDX_TWO]);

    } else {
        ret = sample_svp_npu_case_with_one_arg(*argv[1]);
    }
    if (ret != TD_SUCCESS) {
        sample_svp_npu_usage(argv[0]);
        return TD_FAILURE;
    }

    return 0;
}
