/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>

#include "ot_type.h"
#include "state_machine.h"
#include "state_machine_action.h"
#include "media.h"
#include "media_venc.h"
#include "algorithm.h"
#include "peripheral.h"
#include "kol_timestamp.h"
#include "scene.h"
#include "region.h"


static state_machine_cfg g_state_machine = {0};
static pthread_t g_state_proc_thread_id = 0;
static volatile sig_atomic_t g_sig_flag = 0;
static td_bool g_scene_auto_enable = TD_FALSE;
static td_char g_scene_auto_param[DIR_NAME_BUFF] = {0};

static td_void kol_get_char(td_void)
{
    if (g_sig_flag == 1) {
        return;
    }

    sample_pause();

    g_sig_flag = 1;
}

static td_void kol_handle_sig(td_s32 signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        g_sig_flag = 1;
    }
}

static td_void kol_register_sig_handler(td_void (*sig_handle)(td_s32))
{
    struct sigaction sa;

    (td_void)memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handle;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, TD_NULL);
    sigaction(SIGTERM, &sa, TD_NULL);
}

static kol_start_state kol_get_state(td_void)
{
    if (get_single_state() != START_STATE_BUTT) {
        return get_single_state();
    }
    return get_start_state();
}

static td_void *kol_state_proc_thread(td_void *param)
{
    state_machine_cfg *state_machine = (state_machine_cfg *)param;
    kol_start_state state = state_machine->cur_state = get_start_state();
    prctl(PR_SET_NAME, "kol_state_proc", 0, 0, 0);
    while (g_sig_flag == 0) {
        if (kol_is_need_state_transfer(state_machine, state)) {
            kol_start_event event = kol_get_start_event(state_machine, state);
            state_machine_transfer(state_machine, event);
        }

        state_machine_action(state_machine, media_get_route_num());
        state = kol_get_state();
    }

    return TD_NULL;
}

static td_void kol_usage(const char *prg_name)
{
    printf("usage : %s [-n N] [-k kol_mode] [-s sleep_mode] [-t sleep_time] [-r] [-c] [-p] \n", prg_name);
    printf("option:\n");
    printf("    -h: show help info.\n");
    printf("    -n: set route num. [1, 2, 4]. default: 1\n");
    printf("    -k: set kol mode. 0: auto mode, 1: record mode, 2: kol mode. default: 0\n");
    printf("    -s: set sleep mode. 0: sleep none, 1: light sleep, "
        "2: deep sleep, 3: user sleep. default: 1\n");
    printf("    -t: set sleep time(ms). [100, 5000] || [-1]. default: 1000\n");
    printf("    -r: run ai detect. 0:none, 1:route_0, 2:route_1, 3:route_0 and route_1, "
        "4:route_2, 8:route_3, 15:all routes. default: 0\n");
    printf("    -i: scene auto ini. eg: ./param/sensor_hy006_3814_0011.\n");
    printf("    -d: light on threshold. default: 1000\n");
    printf("    -c: communicate with mcu.\n");
    printf("    -p: print statistics\n");
    printf("eg: %s -n 1 -k 0 -s 0 -t 900 -r 1 -i ./param/sensor_hy006_3814_0011 -c -p\n", prg_name);
}

static td_void kol_set_route_num(const td_char *arg)
{
    td_char *end_ptr = TD_NULL;
    td_u32 route_num = (td_u32)strtol(arg, &end_ptr, 10); /* 10: base */
    if ((end_ptr == arg) || (*end_ptr) != '\0' ||
        (route_num != 1 && route_num != 2 && route_num != 4)) { /* 1,2,4: support route num */
        sample_print("route num %s is illegal, use default value\n", arg);
    } else {
        media_set_route_num(route_num);
    }
}

static td_void kol_set_kol_mode(const td_char *arg)
{
    td_char *end_ptr = TD_NULL;
    td_u32 mode = (td_u32)strtol(arg, &end_ptr, 10); /* 10: base */
    if ((end_ptr == arg) || (*end_ptr) != '\0' || mode > 2) { /* 2: max mode */
        sample_print("mode %s is illegal, use default value\n", arg);
    } else {
        switch (mode) {
            case 1:
                set_single_state(NORMAL_RECORD_STATE);
                break;
            case 2: /* 2 kol mode */
                set_single_state(KOL_STATE);
                break;
            default:
                break;
        }
    }
}

static td_void kol_set_sleep_mode(const td_char *arg)
{
    td_char *end_ptr = TD_NULL;
    td_u32 mode = (td_u32)strtol(arg, &end_ptr, 10); /* 10: base */
    if ((end_ptr == arg) || (*end_ptr) != '\0' || mode > 3) { /* 3: max sleep mode */
        sample_print("sleep mode %s is illegal, use default value\n", arg);
    } else {
        set_kol_sleep_mode(mode);
    }
}

static td_void kol_set_sleep_time(const td_char *arg)
{
    td_char *end_ptr = TD_NULL;
    td_s32 sleep_time = (td_s32)strtol(arg, &end_ptr, 10); /* 10: base */
    /* [100, 5000]sleep time range */
    if ((end_ptr == arg) || (*end_ptr) != '\0' || sleep_time > 5000 || (sleep_time < 100 && sleep_time != -1)) {
        sample_print("sleep time %s is out of range[100, 5000] and -1, use default value\n", arg);
    } else {
        set_kol_sleep_time(sleep_time);
    }
}

static td_void kol_set_aidetect(const td_char *arg)
{
    td_char *end_ptr = TD_NULL;
    td_u32 aidetect_mask = (td_u32)strtol(arg, &end_ptr, 10); /* 10: base */
    if (aidetect_mask >= (1 << MAX_ROUTE_CNT)) {
        sample_print("aidetect_mask(%u) out of range[0, %d) \n", aidetect_mask, (1 << MAX_ROUTE_CNT));
    } else {
        set_aidetect_mask(aidetect_mask);
        media_set_aidetect_mask(aidetect_mask);
    }
}

static td_void kol_set_scene_auto_ini(const td_char *arg)
{
    int ret = strcpy_s(g_scene_auto_param, DIR_NAME_BUFF, arg);
    if (ret != EOK) {
        sample_print("strcpy_s failed.\n");
        return;
    }
    g_scene_auto_enable = TD_TRUE;
}

static td_void kol_set_iso_threshold(const td_char *arg)
{
    td_char *end_ptr = TD_NULL;
    td_s32 threshold = (td_s32)strtol(arg, &end_ptr, 10); /* 10: base */
    /* iso range >= 100 */
    if ((end_ptr == arg) || (*end_ptr) != '\0' || threshold < 100) {
        sample_print("threshold param %s is illegal, should greater than or equal to 100, use default value\n", arg);
    } else {
        set_kol_iso_threshold((td_u32)threshold);
    }
}

static td_s32 kol_parese_argv(td_s32 argc, td_char *argv[])
{
    int result;

    opterr = 0;
    while ((result = getopt(argc, argv, "n:k:s:t:r:i:d:cpHh")) != -1) {
        switch ((char)result) {
            case 'n':
                kol_set_route_num(optarg);
                break;
            case 'k':
                kol_set_kol_mode(optarg);
                break;
            case 's':
                kol_set_sleep_mode(optarg);
                break;
            case 't':
                kol_set_sleep_time(optarg);
                break;
            case 'r':
                kol_set_aidetect(optarg);
                break;
            case 'i':
                kol_set_scene_auto_ini(optarg);
                break;
            case 'd':
                kol_set_iso_threshold(optarg);
                break;
            case 'c':
                set_mcu_switch(TD_TRUE);
                break;
            case 'p':
                set_statistics_switch(TD_TRUE);
                break;
            case 'h':
            case 'H':
                kol_usage(argv[0]);
                return TD_FAILURE;
            case '?': {
                sample_print("unkown option.\n");
                kol_usage(argv[0]);
                return TD_FAILURE;
            }
            default:
                break;
        }
    }

    return TD_SUCCESS;
}

static td_s32 region_init(td_u32 route_num)
{
    td_s32 ret = region_create_overlay(route_num);
    if (ret != TD_SUCCESS) {
        sample_print("region_create_overlay failed \n");
        return ret;
    }

    ret = region_create_corner_rect(OT_RGN_VPSS_MAX_CORNER_RECT_NUM * route_num);
    if (ret != TD_SUCCESS) {
        sample_print("region_create_corner_rect failed \n");
        return ret;
    }

    return TD_SUCCESS;
}

static td_void region_deinit(td_u32 route_num)
{
    region_destroy_corner_rect(OT_RGN_VPSS_MAX_CORNER_RECT_NUM * route_num);

    region_destroy_overlay(route_num);
}

static td_s32 kol_init(td_u32 route_num)
{
    td_s32 ret;

    ret = media_init();
    if (ret != TD_SUCCESS) {
        sample_print("media init failed.\n");
        return ret;
    }

    ret = region_init(route_num);
    if (ret != TD_SUCCESS) {
        sample_print("region init failed.\n");
        goto exit1;
    }

    if (g_scene_auto_enable) {
        ret = scene_init(g_scene_auto_param, route_num);
        if (ret != TD_SUCCESS) {
            sample_print("scene init failed.\n");
            goto exit2;
        }
    }

    ret = algorithm_aidetect_init(route_num);
    if (ret != TD_SUCCESS) {
        sample_print("algorithm aidetect init failed.\n");
        goto exit3;
    }

    ret = state_machine_resource_init(route_num);
    if (ret != TD_SUCCESS) {
        sample_print("state machine resource init failed. 0x%x\n", ret);
        goto exit4;
    }

    return TD_SUCCESS;
exit4:
    algorithm_aidetect_deinit(route_num);
exit3:
    if (g_scene_auto_enable) {
        scene_deinit();
    }
exit2:
    region_deinit(route_num);
exit1:
    media_deinit();
    return ret;
}

static td_void kol_deinit(td_u32 route_num)
{
    state_machine_resource_deinit(route_num);
    pthread_join(g_state_proc_thread_id, TD_NULL);
    algorithm_aidetect_deinit(route_num);
    if (g_scene_auto_enable) {
        scene_deinit();
    }
    region_deinit(route_num);
    media_deinit();
}

td_s32 main(td_s32 argc, td_char *argv[])
{
    kol_register_sig_handler(kol_handle_sig);

    td_s32 ret = TD_SUCCESS;
    ret = kol_parese_argv(argc, argv);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    ot_syskol_sleep_mode kol_sleep_mode = get_kol_sleep_mode();
    if (kol_sleep_mode == OT_SYSKOL_SLEEP_MODE_LIGHT) {
        kol_rtsp_server_start();
    }

    td_u32 route_num = media_get_route_num();
    ret = kol_init(route_num);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    state_machine_init(&g_state_machine, get_action_map(), get_event_map());

    ret = pthread_create(&g_state_proc_thread_id, TD_NULL, kol_state_proc_thread, (td_void *)&g_state_machine);
    if (ret != TD_SUCCESS) {
        sample_print("create state proc thread failed.\n");
        kol_deinit(route_num);
        return ret;
    }

    kol_get_char();

    if (kol_sleep_mode == OT_SYSKOL_SLEEP_MODE_LIGHT) {
        kol_rtsp_server_stop(kol_sleep_mode);
    }

    set_single_state(START_STATE_BUTT);
    set_start_state(START_STATE_BUTT);
    kol_deinit(route_num);
    return 0;
}
